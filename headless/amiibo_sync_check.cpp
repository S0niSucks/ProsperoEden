// SPDX-License-Identifier: GPL-3.0-or-later
// Synthetic pixels and public-ID-shaped strings only. No tag/key bytes or third-party art.
#include "amiibo_sync.h"
#include <array>
#include <cassert>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <thread>
#include <sys/stat.h>
#include <sys/file.h>
#include <fcntl.h>
#include <unistd.h>
#include <zlib.h>
#include <curl/curl.h>

namespace fs = std::filesystem;
const std::string catalog = R"({"amiibo":[{"head":"00000000","tail":"00000001","name":"Synthetic One","amiiboSeries":"Fixture"},{"head":"00000000","tail":"00000002","name":"Synthetic Two"}]})";
void Big(std::string& out, unsigned n) { for (int i = 3; i >= 0; --i) out += static_cast<char>(n >> (i * 8)); }
void Chunk(std::string& out, const char* kind, std::string body) {
    Big(out, body.size()); const auto start = out.size(); out.append(kind, 4); out += body;
    Big(out, crc32(0, reinterpret_cast<const Bytef*>(out.data() + start), 4 + body.size()));
}
std::string Png(unsigned char red = 20) {
    std::string out("\x89PNG\r\n\x1a\n", 8), ihdr;
    Big(ihdr, 1); Big(ihdr, 1); ihdr.append("\x08\x06\0\0\0", 5); Chunk(out, "IHDR", ihdr);
    const unsigned char pixels[]{0, red, 40, 60, 255};
    unsigned char compressed[100]{}; uLongf size = sizeof(compressed);
    assert(compress(compressed, &size, pixels, sizeof(pixels)) == Z_OK);
    Chunk(out, "IDAT", std::string(reinterpret_cast<char*>(compressed), size)); Chunk(out, "IEND", {}); return out;
}
void Write(const fs::path& path, const std::string& bytes) { std::ofstream(path, std::ios::binary) << bytes; }
std::string Read(const fs::path& path) { std::ifstream in(path, std::ios::binary); return {std::istreambuf_iterator<char>(in), {}}; }
template <class F> void Reject(F f) { bool rejected = false; try { f(); } catch (...) { rejected = true; } assert(rejected); }
std::atomic<int> mode{0}, calls{0};
fs::path race_image;
std::string SyntheticFetch(const std::string& url, std::size_t, const std::atomic<bool>& stop, Eden::AmiiboSyncProgress progress) {
    ++calls;
    if (mode == 1) { while (!stop) std::this_thread::sleep_for(std::chrono::milliseconds(1)); throw std::runtime_error("cancel fixture"); }
    if (mode == 2 && url.find("00000002.png") != url.npos) return "bad png";
    if (mode == 3) { while (mode == 3 && !stop) std::this_thread::sleep_for(std::chrono::milliseconds(1)); }
    if (mode == 4 && url.ends_with(".png")) Write(race_image, Png(99));
    if (progress) progress(1, 1);
    return url.ends_with(".png") ? Png() : catalog;
}
Eden::AmiiboSyncStatus Wait(Eden::AmiiboSync& sync) {
    for (int i = 0; i < 10000; ++i) {
        auto s = sync.Status(); if (!s.busy) return s;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    assert(false && "worker deadline"); return {};
}
void NoTemps(const fs::path& root) {
    for (const auto& item : fs::recursive_directory_iterator(root))
        assert(!item.path().filename().string().starts_with(".amiibo-sync-"));
}
using Labels = std::vector<std::array<std::string, 3>>;
Labels LabelsOf(const Eden::AmiiboSyncStatus& status) {
    Labels labels;
    for (const auto& e : status.entries) labels.push_back({e.id, e.name, e.series});
    return labels;
}
void CatalogViews(const fs::path& root) {
    const auto folder = root / "views";
    fs::create_directory(folder);
    const auto downloaded = folder / "catalog-sync.json", local = folder / "amiibo.json";
    const std::string custom = R"({"amiibos":{"0x0000000000000001":{"name":"My One"},"0x1000000000000001":{"name":"My Other Head"}},"amiibo_series":{"0x00":"My Series"}})";
    const Labels remote{{"0000000000000001", "Synthetic One", "Fixture"}, {"0000000000000002", "Synthetic Two", ""}};
    const Labels mine{{"0000000000000001", "My One", "My Series"}, {"1000000000000001", "My Other Head", "My Series"}};
    const Labels merged{mine[0], remote[1], mine[1]};
    Write(root / "remote-link-target.json", catalog); Write(root / "local-link-target.json", custom);
    Eden::AmiiboSync sync(SyntheticFetch);
    Eden::AmiiboSyncRequest request; request.folder = folder.string(); request.offline = true;
    const auto offline = [&] {
        const auto before_calls = calls.load(); assert(sync.Start(request)); auto status = Wait(sync);
        assert(calls == before_calls); return status;
    };
    const auto expect = [&](const Labels& labels) {
        const auto status = offline(); assert(status.phase == "ready" && status.error.empty());
        assert(LabelsOf(status) == labels);
    };
    Write(downloaded, catalog); Write(local, custom);
    expect(merged); // overlapping/disjoint IDs, same tail but different head, unique sorted full IDs
    assert(Read(downloaded) == catalog && Read(local) == custom);
    request.offline = false; request.confirmed = true;
    assert(sync.Start(request)); auto status = Wait(sync);
    assert(status.phase == "ready" && LabelsOf(status) == merged);
    const auto normalized = Read(downloaded);
    Eden::AmiiboSyncStatus saved; saved.entries = Eden::ParseAmiiboSyncCatalog(normalized);
    assert(LabelsOf(saved) == remote);
    assert(Read(local) == custom); // completed status only, never persist user overrides to the cache
    request.offline = true;
    Write(local, R"({"amiibo":[{"head":"00000000","tail":"00000001","name":"Only Name"}]})");
    expect({{"0000000000000001", "Only Name", ""}, remote[1]}); // local absent series clears remote series
    Write(local, custom); Write(downloaded, "{"); expect(mine); // bad sync / good local
    Write(downloaded, catalog); Write(local, "{"); expect(remote); // bad local / good sync
    fs::remove(local); expect(remote);
    Write(local, custom); fs::remove(downloaded); expect(mine);
    // Rejected sources are isolated, not followed/opened as ordinary files. A safe peer still wins.
    fs::create_symlink(root / "remote-link-target.json", downloaded); expect(mine);
    fs::remove(downloaded); assert(mkfifo(downloaded.c_str(), 0600) == 0); expect(mine);
    fs::remove(downloaded); fs::create_directory(downloaded); expect(mine);
    fs::remove(downloaded); Write(downloaded, std::string(2 * 1024 * 1024 + 1, ' ')); expect(mine);
    Write(downloaded, catalog); fs::remove(local);
    fs::create_symlink(root / "local-link-target.json", local); expect(remote);
    // No usable source is a failed refresh, not a successful empty list. Preserve the last good view.
    const auto failure = [&] {
        const auto previous = LabelsOf(sync.Status()); const auto status = offline();
        assert(status.phase == "failed" && LabelsOf(status) == previous);
        assert(status.error.starts_with("No usable offline Amiibo catalog; catalog-sync.json: "));
        assert(status.error.find("; amiibo.json: ") != status.error.npos);
        return status.error;
    };
    fs::remove(downloaded); // even a link to a valid regular catalog is not a usable source
    assert(failure().find("Cannot open a safe Amiibo cache path") != std::string::npos);
    fs::remove(local); auto error = failure();
    assert(error == "No usable offline Amiibo catalog; catalog-sync.json: missing or empty; amiibo.json: missing or empty");
    Write(downloaded, ""); Write(local, R"({"amiibo":[]})");
    assert(failure().find("Amiibo catalog is empty") != std::string::npos);
    Write(downloaded, "{"); Write(local, "{"); failure();
    // A refresh's new labels must never leak into a failed/cancelled snapshot.
    Write(downloaded, catalog); Write(local, custom); expect(merged);
    Write(local, R"({"amiibo":[{"head":"00000000","tail":"00000001","name":"Do Not Publish"}]})");
    request.offline = false; request.ids = {"ffffffffffffffff"};
    assert(sync.Start(request)); status = Wait(sync);
    assert(status.phase == "failed" && LabelsOf(status) == merged && Read(downloaded) == catalog);
    request.ids.clear(); mode = 1; calls = 0; assert(sync.Start(request));
    while (!calls) std::this_thread::yield();
    assert(LabelsOf(sync.Status()) == merged); sync.Cancel(); status = Wait(sync);
    assert(status.phase == "cancelled" && LabelsOf(status) == merged && Read(downloaded) == catalog);
    mode = 0;
    // Bounds apply per source: preserve the entire disjoint union and make every ID pageable.
    const auto large = [](unsigned head) {
        std::string text = "{\"amiibo\":[";
        for (unsigned n = 0; n < 4096; ++n) {
            char item[160]; std::snprintf(item, sizeof(item),
                "%s{\"head\":\"%08x\",\"tail\":\"%08x\",\"name\":\"Bounded\"}", n ? "," : "", head, n);
            text += item;
        }
        return text + "]}";
    };
    request.offline = true; Write(downloaded, large(0)); Write(local, large(1));
    status = offline(); assert(status.phase == "ready" && status.entries.size() == 8192);
    for (std::size_t i = 1; i < status.entries.size(); ++i) assert(status.entries[i - 1].id < status.entries[i].id);
    // An unsafe collection root cannot be bypassed even when its target contains usable catalogs.
    fs::create_directory_symlink(folder, root / "views-link");
    request.offline = true; request.folder = (root / "views-link").string();
    assert(sync.Start(request)); status = Wait(sync); assert(status.phase == "failed" && status.entries.empty());
    sync.Stop(); NoTemps(folder);
    std::puts("Amiibo catalog views PASS: bounded independent sources, full-ID union, local name/series overrides, online cache separation, malformed/missing/empty/unsafe fallback, failure/cancel snapshots");
}
extern "C" void console_curl_setup(CURL* easy) {
    curl_easy_setopt(easy, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(easy, CURLOPT_CAINFO, std::getenv("AMIIBO_TEST_CA"));
}
int main(int argc, char** argv) {
    char temporary[] = "/tmp/eden-amiibo-worker-XXXXXX"; assert(mkdtemp(temporary));
    const fs::path root = temporary;
    if (argc > 1) {
        assert(argc == 4);
        Eden::AmiiboSync sync;
        Eden::AmiiboSyncRequest request;
        request.folder = (root / "amiibo").string(); request.confirmed = true;
        request.source = argv[1];
        request.image_template = request.source.substr(0, request.source.find('/', 8)) + "/icon_{head}-{tail}.png";
        request.ids = {"0000000000000001"};
        assert(sync.Start(request));
        const auto began = std::chrono::steady_clock::now();
        if (std::atoi(argv[3])) { std::this_thread::sleep_for(std::chrono::milliseconds(100)); sync.Cancel(); }
        const auto result = Wait(sync);
        if (result.phase != argv[2]) std::fprintf(stderr, "Expected %s, got %s (%s)\n", argv[2], result.phase.c_str(), result.error.c_str());
        assert(result.phase == argv[2]);
        if (std::atoi(argv[3])) assert(std::chrono::steady_clock::now() - began < std::chrono::seconds(5));
        if (result.phase == "ready") {
            assert(result.entries.size() == 2 && result.photos == 1);
            request.offline = true; assert(sync.Start(request)); assert(Wait(sync).entries.size() == 2);
            assert(fs::exists(root / "amiibo/images/icon_00000000-00000001.png"));
        } else assert(!fs::exists(root / "amiibo/catalog-sync.json"));
        sync.Stop(); NoTemps(root); fs::remove_all(root); return 0;
    }
    CatalogViews(root);
    calls = 0;
    Eden::ValidateAmiiboSyncPng(Png());
    auto corrupt = Png(); corrupt[40] ^= 1; Reject([&] { Eden::ValidateAmiiboSyncPng(corrupt); });
    Reject([&] { Eden::ValidateAmiiboSyncPng(Png() + "trailing"); });
    assert(Eden::ParseAmiiboSyncCatalog(catalog).size() == 2);
    Reject([&] { Eden::ParseAmiiboSyncCatalog(R"({"amiibo":[],"amiibo":[]})"); });
    Reject([&] { Eden::ParseAmiiboSyncCatalog(R"({"amiibo":[{"head":"00000000","tail":"00000001","name":"A"},{"head":"00000000","tail":"00000001","name":"B"}]})"); });
    Reject([&] { Eden::ParseAmiiboSyncCatalog(std::string(2 * 1024 * 1024 + 1, ' ')); });
    Reject([&] { Eden::AmiiboSyncId("../../bad/path"); });
    fs::create_directories(root / "amiibo/images");
    Write(root / "amiibo/amiibo.json", "custom metadata preserved byte-for-byte");
    // A FIFO named like a dump/key proves that synchronization never even opens them.
    assert(mkfifo((root / "amiibo/tag.bin").c_str(), 0600) == 0);
    assert(mkfifo((root / "amiibo/key_retail.bin").c_str(), 0600) == 0);
    Eden::AmiiboSync sync(SyntheticFetch);
    Eden::AmiiboSyncRequest request; request.folder = (root / "amiibo").string();
    assert(!sync.Start(request)); assert(calls == 0); // backend confirmation, not just UI
    request.confirmed = true; request.ids = {"0000000000000001", "0000000000000002"};
    mode = 2; assert(sync.Start(request)); assert(Wait(sync).phase == "failed");
    assert(!fs::exists(root / "amiibo/catalog-sync.json"));
    assert(!fs::exists(root / "amiibo/images/icon_00000000-00000001.png")); NoTemps(root);
    mode = 0; assert(sync.Start(request)); auto result = Wait(sync); assert(result.phase == "ready" && result.photos == 2);
    assert(Read(root / "amiibo/amiibo.json") == "custom metadata preserved byte-for-byte");
    calls = 0; assert(sync.Start(request)); result = Wait(sync); assert(result.reused == 2 && result.photos == 0 && calls == 1);
    request.offline = true; calls = 0; assert(sync.Start(request)); assert(Wait(sync).entries.size() == 2 && calls == 0);
    request.offline = false;
    Write(root / "amiibo/images/icon_00000000-00000001.png", "custom invalid photo still kept");
    assert(sync.Start(request)); assert(Wait(sync).phase == "failed");
    assert(Read(root / "amiibo/images/icon_00000000-00000001.png") == "custom invalid photo still kept");
    const auto before = Read(root / "amiibo/catalog-sync.json");
    mode = 1; calls = 0; assert(sync.Start(request));
    while (!calls) std::this_thread::yield();
    assert(!sync.Start(request)); sync.Cancel(); assert(Wait(sync).phase == "cancelled");
    assert(Read(root / "amiibo/catalog-sync.json") == before); NoTemps(root);
    // A caller can mutate/destroy its config while the worker blocks: request/path are value-owned.
    request.ids.clear(); mode = 3; calls = 0; assert(sync.Start(request));
    while (!calls) std::this_thread::yield();
    request.folder = (root / "changed").string(); request.source = "https://changed.invalid/";
    mode = 0; assert(Wait(sync).phase == "ready"); assert(!fs::exists(root / "changed"));
    // Cache descriptors cannot follow a symlink in any ancestor, nor a FIFO as a cache target.
    fs::create_directory_symlink(root / "amiibo", root / "link");
    request.folder = (root / "link/cache").string(); assert(sync.Start(request)); assert(Wait(sync).phase == "failed");
    request.folder = (root / "amiibo").string();
    fs::remove(root / "amiibo/catalog-sync.json");
    assert(mkfifo((root / "amiibo/catalog-sync.json").c_str(), 0600) == 0);
    assert(sync.Start(request)); assert(Wait(sync).phase == "failed");
    fs::remove(root / "amiibo/catalog-sync.json");
    fs::create_symlink(root / "amiibo/amiibo.json", root / "amiibo/catalog-sync.json");
    assert(sync.Start(request)); assert(Wait(sync).phase == "failed");
    fs::remove(root / "amiibo/catalog-sync.json");
    const int held = open((root / "amiibo/.amiibo-sync.lock").c_str(), O_RDWR);
    assert(held >= 0 && flock(held, LOCK_EX | LOCK_NB) == 0);
    assert(sync.Start(request)); assert(Wait(sync).phase == "failed"); close(held);
    // A custom photo created after preflight still wins the atomic create-only publication.
    request.ids = {"0000000000000001"};
    race_image = root / "amiibo/images/icon_00000000-00000001.png"; fs::remove(race_image);
    mode = 4; assert(sync.Start(request)); result = Wait(sync);
    assert(result.phase == "ready" && result.photos == 0 && result.reused == 1);
    assert(Read(race_image) == Png(99));
    fs::remove(race_image); fs::create_symlink(root / "amiibo/amiibo.json", race_image);
    mode = 0; assert(sync.Start(request)); assert(Wait(sync).phase == "failed");
    fs::remove(race_image); assert(mkfifo(race_image.c_str(), 0600) == 0);
    assert(sync.Start(request)); assert(Wait(sync).phase == "failed");
    fs::remove(race_image);
    request.ids = {"ffffffffffffffff"};
    assert(sync.Start(request)); assert(Wait(sync).phase == "failed");
    request.ids.assign(65, "0000000000000001");
    calls = 0; assert(sync.Start(request)); assert(Wait(sync).phase == "failed" && calls == 0);
    request.ids.clear();
    sync.Stop(); assert(!sync.Start(request)); sync.AllowStart();
    mode = 1; calls = 0; assert(sync.Start(request));
    while (!calls) std::this_thread::yield();
    sync.Stop(); assert(sync.Status().phase == "cancelled");
    NoTemps(root); fs::remove_all(root);
    std::puts("Amiibo synthetic worker PASS: confirmation, strict formats, no dump/key opens, batch failure, cancel, offline, no-clobber, safe paths and value-owned config");
}
