// SPDX-License-Identifier: GPL-3.0-or-later
#include "shares.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <future>
#include <iostream>
#include <random>
#include <thread>
#include <vector>
#include <sys/stat.h>

using namespace Eden::Network;
using Clock = std::chrono::steady_clock;
constexpr std::uint64_t kSize = (5ull << 30) + 65536;
unsigned char Byte(std::uint64_t offset) {
    return offset >= (4u << 20) && offset < (5ull << 30) ? 0 :
        static_cast<unsigned char>((offset * 13 + (offset >> 17)) % 251);
}
void CheckRead(Session& s, const Entry& file, std::uint64_t offset, std::size_t size) {
    std::vector<unsigned char> bytes(size, 0xFE);
    const auto expected = std::min<std::uint64_t>(size, file.size - std::min(file.size, offset));
    const auto got = s.Read(file, bytes.data(), size, offset);
    if (got != expected) std::cerr << "Read mismatch offset=" << offset << " size=" << size << " got=" << got << " " << s.Error() << '\n';
    assert(got == expected);
    for (std::size_t at = 0; at < expected; ++at) assert(bytes[at] == Byte(offset + at));
    for (std::size_t at = expected; at < size; ++at) assert(bytes[at] == 0xFE);
}
int main(int argc, char** argv) {
    assert(argc == 6);
    Share share;
    share.id = "test"; share.name = "Fixture"; share.protocol = argv[1]; share.server = "127.0.0.1";
    share.port = std::stoi(argv[2]); share.share = argv[3]; share.mount_port = std::stoi(argv[4]);
    share.timeout_ms = 1500; share.cache_bytes = 1u << 20;
    const std::filesystem::path work = argv[5];
    assert(Valid(share));
    for (const auto* path : {"../escape", "a/../b", "/absolute", "a//b", "a/", "a\\b", "a:b", "a%2fb", "a\n", ".", ".."})
        assert(!SafePath(path));
    assert(SafePath("folder/Game [0100000000000000].nsp"));
    assert(Uri("test", "Game.nsp") == "pe-net://test/Game.nsp");
    assert(Uri("test", "../secret").empty());
    Share invalid = share; invalid.server = "user:secret@host"; assert(!Valid(invalid));
    invalid = share; invalid.folder = "../escape"; assert(!Valid(invalid));
    invalid = share; invalid.server = std::string{"127.0.0.1\0hidden", 16}; assert(!Valid(invalid));
    if (share.protocol == "nfs") { invalid = share; invalid.encrypt = true; assert(!Valid(invalid)); }

    Share denied = share; denied.share = share.protocol == "smb" ? "no-such-share" : "/denied";
    if (share.protocol == "smb") { denied.user = "NO_SUCH_SYNTHETIC_USER"; denied.password = "NEVER_SHOW_THIS"; }
    Session inaccessible(denied); assert(!inaccessible.Connect());
    assert(!inaccessible.Error().empty() && inaccessible.Error().find("NEVER_SHOW_THIS") == std::string::npos);

    auto s = std::make_shared<Session>(share);
    if (!s->Connect()) { std::cerr << s->Error() << '\n'; return 1; }
    std::vector<Entry> entries;
    assert(s->List("", &entries));
    assert(std::any_of(entries.begin(), entries.end(), [](const Entry& e) { return e.path == "Game.nsp" && e.size == kSize; }));
    Entry file;
    assert(s->Stat("Game.nsp", &file) && file.size == kSize);
    CheckRead(*s, file, 47, 8192);
    auto reads = s->TransportReads();
    CheckRead(*s, file, 47, 8192); assert(s->TransportReads() == reads);
    CheckRead(*s, file, (256u << 10) - 17, 4096);
    CheckRead(*s, file, (5ull << 30) + 7, 30000);
    CheckRead(*s, file, kSize - 13, 4096);
    CheckRead(*s, file, kSize, 32);
    std::vector<std::future<void>> threads;
    for (unsigned t = 0; t < 4; ++t) threads.push_back(std::async(std::launch::async, [&, t] {
        std::mt19937 random(171 + t);
        for (unsigned n = 0; n < 24; ++n) CheckRead(*s, file, random() % (4u << 20), 1 + random() % 32000);
    }));
    for (auto& thread : threads) thread.get();
    assert(s->CachedBytes() <= share.cache_bytes);
    assert(!s->Stat("../escape", &file));
    s->Cancel();
    std::array<unsigned char, 32> unchanged; unchanged.fill(0xAB);
    assert(s->Read(file, unchanged.data(), unchanged.size(), 0) == 0);
    assert(!s->Healthy());
    for (auto byte : unchanged) assert(byte == 0xAB);
    assert(s->Error().find("Reconnect") != std::string::npos);
    Session reconnected(share); assert(reconnected.Connect());
    assert(reconnected.Stat("Game.nsp", &file)); CheckRead(reconnected, file, 1023, 127);

    // A real library transport stalled before negotiation: bounded cancellation and timeout.
    Share stalled = share; stalled.port = 19998; stalled.mount_port = 19998; stalled.timeout_ms = 400;
    auto pending = std::make_shared<Session>(stalled);
    auto start = Clock::now();
    auto call = std::async(std::launch::async, [&] { return pending->Connect(); });
    std::this_thread::sleep_for(std::chrono::milliseconds(60)); pending->Cancel();
    assert(!call.get());
    assert(Clock::now() - start < std::chrono::milliseconds(600));
    Session timeout(stalled); start = Clock::now(); assert(!timeout.Connect());
    assert(Clock::now() - start < std::chrono::milliseconds(1000));

    if (share.protocol == "nfs") {
        auto reading = std::make_shared<Session>(share); assert(reading->Connect());
        Entry live; assert(reading->Stat("Game.nsp", &live));
        unchanged.fill(0xAB); start = Clock::now();
        auto io = std::async(std::launch::async, [&] { return reading->Read(live, unchanged.data(), unchanged.size(), 2ull << 30); });
        std::this_thread::sleep_for(std::chrono::milliseconds(60)); reading->Cancel();
        assert(io.get() == 0 && Clock::now() - start < std::chrono::milliseconds(600));
        for (auto byte : unchanged) assert(byte == 0xAB);
    }

    const auto config = work / (share.protocol + "-shares.json");
    {
        std::ofstream out(config);
        out << "{\"shares\":[{\"id\":\"test\",\"protocol\":\"" << share.protocol << "\",\"server\":\"127.0.0.1\",\"share\":\""
            << share.share << "\",\"port\":" << share.port << ",\"mount_port\":" << share.mount_port << ",\"cache_mib\":1}]}";
    }
    Configure(config.string());
    for (unsigned n = 0; n < 300 && Current().busy; ++n) std::this_thread::sleep_for(std::chrono::milliseconds(10));
    assert(!Current().busy && Current().shares.at(0).connected);
    assert(Known("pe-net://test/Game.nsp"));
    auto games = Games(); assert(!games.empty());
    struct stat mode{}; assert(stat(config.c_str(), &mode) == 0 && (mode.st_mode & 0777) == 0600);
    BeginGame();
    auto opened = Open("pe-net://test/Game.nsp"); assert(opened.session);
    StopDiscovery(); // menu gone; the consumer still reads
    CheckRead(*opened.session, opened.entry, 65531, 8192);
    assert(GameError().empty());
    CancelGame(); assert(!GameError().empty());
    assert(opened.session->Read(opened.entry, unchanged.data(), unchanged.size(), 0) == 0);
    EndGame(); Refresh();
    for (unsigned n = 0; n < 300 && Current().busy; ++n) std::this_thread::sleep_for(std::chrono::milliseconds(10));
    assert(Known("pe-net://test/Game.nsp"));
    auto fresh = Open("pe-net://test/Game.nsp"); assert(fresh.session && fresh.session != opened.session);
    CheckRead(*fresh.session, fresh.entry, 65531, 8192);
    assert(opened.session->Read(opened.entry, unchanged.data(), unchanged.size(), 0) == 0);
    Disconnect(); assert(!Known("pe-net://test/Game.nsp"));
    { std::ofstream out(config); out << "{\"password\":\"NEVER_SHOW_THIS\","; }
    Configure(config.string());
    StopDiscovery();
    assert(!Current().error.empty() && Current().error.find("NEVER_SHOW_THIS") == std::string::npos);
    Disconnect();
    const auto link = work / (share.protocol + "-config-link.json");
    std::filesystem::remove(link); std::filesystem::create_symlink(config.filename(), link);
    Configure(link.string()); StopDiscovery(); assert(!Current().error.empty());
    const auto fifo = work / (share.protocol + "-config-fifo");
    std::filesystem::remove(fifo); assert(mkfifo(fifo.c_str(), 0600) == 0);
    start = Clock::now(); Configure(fifo.string()); StopDiscovery();
    assert(!Current().error.empty() && Clock::now() - start < std::chrono::milliseconds(600));
    { std::ofstream out(config); out << std::string(65537, 'x'); }
    Configure(config.string()); StopDiscovery(); assert(!Current().error.empty());
    Disconnect();

    if (share.protocol == "smb") {
        Session truncate(share); assert(truncate.Connect());
        Entry mutable_file; assert(truncate.Stat("Truncate.nsp", &mutable_file));
        CheckRead(truncate, mutable_file, 0, 16);
        std::filesystem::resize_file(work / "share/Truncate.nsp", 1);
        unchanged.fill(0xAB);
        assert(truncate.Read(mutable_file, unchanged.data(), unchanged.size(), 2u << 20) == 0);
        assert(!truncate.Healthy());
        for (auto byte : unchanged) assert(byte == 0xAB);

        // A new consumer cannot accidentally receive a previous file identity's cached bytes.
        Session replaced(share); assert(replaced.Connect());
        Entry original; assert(replaced.Stat("Game.nsp", &original));
        CheckRead(replaced, original, 0, 16);
        auto changed = original; ++changed.modified;
        unchanged.fill(0xAB);
        assert(replaced.Read(changed, unchanged.data(), unchanged.size(), 0) == 0 && !replaced.Healthy());
        for (auto byte : unchanged) assert(byte == 0xAB);
    }
    std::cout << share.protocol << ": random/64-bit/EOF/concurrent reads, cache bound/hit, cancellation, timeout, discovery, consumer lifetime, reconnect, denied access, failure and secret redaction PASS\n";
}
