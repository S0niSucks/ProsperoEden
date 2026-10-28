// SPDX-License-Identifier: GPL-3.0-or-later
// Exact remote implementation/threads. Synthetic source bytes and held OS cleanup, no network.
#include <atomic>
#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <thread>
#include <unistd.h>
#include "remote_worker_source.inc"
using namespace std::chrono_literals;
namespace fs = std::filesystem;
namespace {
std::atomic<bool> cover_entered{}, release_cover{}, remove_entered{}, release_remove{}, hold_remove{};
std::string staged_file;
template<class F> void Until(F ready) {
    const auto end = std::chrono::steady_clock::now() + 8s;
    while (!ready() && std::chrono::steady_clock::now() < end) std::this_thread::sleep_for(1ms);
    assert(ready());
}
void Write(const std::string& path, const std::string& bytes) {
    fs::create_directories(fs::path(path).parent_path());
    std::ofstream out(path); out << bytes; assert(out.good());
}
bool Cover(const std::string& bytes, const std::string& path) { Write(path, bytes); return true; }
class Source final : public Eden::Remote::Source {
public:
    std::string address() const override { return "synthetic-offline"; }
    bool list(std::vector<Eden::Remote::SourceGame>* out, std::string*, const Eden::Remote::Stopped&) override {
        Eden::Remote::SourceGame game; game.id = "local"; game.name = "Synthetic"; game.cover = "local-cover";
        game.files.push_back({"file", "local.nsp", 4, Eden::Remote::FileKind::game});
        out->push_back(game); return true;
    }
    bool cover(const Eden::Remote::SourceGame&, std::string* out, const Eden::Remote::Stopped&) override {
        cover_entered = true;
        // Deliberately slow completion after halt. Stop is not a synchronous transport barrier.
        while (!release_cover) std::this_thread::sleep_for(1ms);
        *out = "synthetic cover bytes"; return true;
    }
    bool fetch(const Eden::Remote::SourceGame&, const Eden::Remote::SourceFile&, std::uint64_t,
               Eden::Remote::Receiver&, std::string*) override { assert(false); return false; }
};
}
namespace Eden::Remote {
std::unique_ptr<Source> MakeSource(const std::string&, const nlohmann::json&, std::string*) {
    return std::make_unique<::Source>();
}
}
extern "C" int __real_remove(const char*);
extern "C" int __wrap_remove(const char* path) {
    if (hold_remove && path == staged_file) {
        remove_entered = true;
        while (!release_remove) std::this_thread::sleep_for(1ms);
    }
    return __real_remove(path);
}
int main() {
    using namespace Eden::Remote;
    char temporary[] = "/work/remote-snapshot-XXXXXX";
    assert(mkdtemp(temporary));
    const std::string root = temporary;
    const Paths paths{root + "/config", root + "/covers", root + "/roms", root + "/updates", root + "/downloads"};
    Write(paths.config + "/sources.json", R"({"sources":[{"type":"synthetic","name":"Local"}]})");
    Start(paths, Cover, nullptr); Until([] { return cover_entered.load(); });
    const auto key = Current().sources.at(0).key;
    const auto cover = CoverPath(State_(), key, "local");
#ifdef EXPECT_BASE_GAP
    Stop(); // actual bounded three-second return with an unfinished writer
    assert(!fs::exists(cover));
    release_cover = true;
    Until([&] { const std::lock_guard lock(State_().lock); return !State_().listing; });
    assert(fs::exists(cover));
    std::puts("PROVEN exact-base Remote::Stop returns before cover writer publication");
#else
    const std::atomic<bool> cancelled{true}, keep_waiting{false};
    assert(!StopForSnapshot(cancelled) && !fs::exists(cover));
    std::thread drain([&] { assert(StopForSnapshot(keep_waiting)); assert(fs::exists(cover)); });
    release_cover = true; drain.join();
#endif
    staged_file = StagedFolder(paths, "fixture", "discard") + "/part.nsp";
    Write(staged_file, "synthetic staged data");
    hold_remove = true;
    {
        auto& s = State_(); const std::lock_guard lock(s.lock);
        s.discards.push_back({"fixture", "discard", {"part.nsp"}}); s.wake.notify_all();
    }
    Until([] { return remove_entered.load(); }); // actual DownloadThread unlocked inside RemoveStaged
#ifdef EXPECT_BASE_GAP
    Stop(); assert(fs::exists(staged_file));
    release_remove = true; Until([] { return !fs::exists(staged_file); });
    std::puts("PROVEN exact-base Remote::Stop ignores unlocked discard cleanup after halt");
#else
    assert(!StopForSnapshot(cancelled) && fs::exists(staged_file));
    std::thread drain_discard([&] { assert(StopForSnapshot(keep_waiting)); assert(!fs::exists(staged_file)); });
    release_remove = true; drain_discard.join();
    assert(StopForSnapshot(keep_waiting));
    std::puts("Remote snapshot PASS: actual list/cover/download-discard threads and files; cancellation refuses drain, success waits through publication/cleanup, idle drain succeeds");
#endif
    // Detached production threads are idle at process exit; State_ is process-lifetime storage.
}
