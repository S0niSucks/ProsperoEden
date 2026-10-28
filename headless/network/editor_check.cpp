// SPDX-License-Identifier: GPL-3.0-or-later
#include "editor.h"
#include "system_keyboard.h"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sys/stat.h>
#include <sys/resource.h>
#include <csignal>
#include <thread>

namespace {
Eden::TextRequest last;
Eden::TextAnswer next;
std::string Bytes(const std::filesystem::path& file) {
    std::ifstream input(file, std::ios::binary); return {std::istreambuf_iterator<char>(input), {}};
}
void Put(const std::filesystem::path& file, const std::string& bytes) { std::ofstream(file, std::ios::binary) << bytes; }
}
namespace Eden {
TextAnswer AskSystemKeyboard(const TextRequest& request, const std::atomic<bool>&) { last = request; return next; }
}
int main(int argc, char** argv) {
    assert(argc == 2);
    namespace fs = std::filesystem;
    using namespace Eden::Network;
    const auto root = fs::path{argv[1]}; fs::create_directories(root / "config");
    const auto file = (root / "config/network-shares.json").string();
    Editor editor; std::atomic<bool> stop{false};
    const auto enter = [&](Field field, std::u16string value) {
        next = {Eden::TextOutcome::accepted, std::move(value)}; return editor.Enter(field, stop);
    };
    const auto no_stages = [&] {
        for (const auto& entry : fs::directory_iterator(root / "config"))
            assert(!entry.path().filename().string().starts_with("network-shares.json.tmp-"));
    };
    assert(editor.Open(file) == EditorResult::ok && editor.Begin("") == EditorResult::ok);
    assert(editor.Save(stop) == EditorResult::required && !fs::exists(file));
    assert(enter(Field::server, u"host.invalid") == EditorResult::invalid);
    assert(enter(Field::server, u"127.0.0.1") == EditorResult::ok);
    assert(enter(Field::share, u"games") == EditorResult::ok);
    assert(enter(Field::name, u"Synthetic NAS \u00e9") == EditorResult::ok);
    assert(enter(Field::folder, u"../escape") == EditorResult::invalid);
    assert(enter(Field::folder, u"roms") == EditorResult::ok);
    assert(enter(Field::updates, u"addons") == EditorResult::ok);
    assert(enter(Field::port, u"65536") == EditorResult::invalid);
    assert(enter(Field::timeout, u"249") == EditorResult::invalid);
    assert(enter(Field::cache, u"0") == EditorResult::invalid);
    assert(enter(Field::cache, u"32") == EditorResult::ok);
    assert(enter(Field::user, u"fixture-user") == EditorResult::ok);
    assert(enter(Field::domain, u"FIXTURE") == EditorResult::ok);
    assert(enter(Field::password, u"Synthetic-only-password") == EditorResult::ok);
    assert(last.password && last.initial.empty() && last.max_length == 256);
    for (const auto& row : editor.View().rows) assert(row.value.find("Synthetic-only-password") == std::string::npos);
    assert(editor.Change(Field::encrypt) == EditorResult::ok);
    assert(editor.Save(stop) == EditorResult::ok);
    auto config = ReadConfiguration(file);
    assert(config.shares.at(0).password == "Synthetic-only-password" && config.shares.at(0).encrypt);
    assert(config.shares[0].name == "Synthetic NAS \xc3\xa9" && config.shares[0].cache_bytes == (32u << 20));
    struct stat st{}; assert(stat(file.c_str(), &st) == 0 && (st.st_mode & 0777) == 0600); no_stages();
    const auto id = config.shares[0].id, original = Bytes(file);
    Configure(file, false);
    assert(!Current().busy && Current().shares.size() == 1 && !Current().shares[0].connected);
    Disconnect();
    assert(editor.Begin(id) == EditorResult::ok);
    next = {Eden::TextOutcome::cancelled, u"Never-used"};
    assert(editor.Enter(Field::password, stop) == EditorResult::cancelled && last.initial.empty());
    assert(enter(Field::password, u"Replacement-only-secret") == EditorResult::ok);
    assert(editor.Password(PasswordChoice::keep) == EditorResult::ok);
    assert(editor.Save(stop) == EditorResult::ok && Bytes(file) == original);
    assert(editor.Begin(id) == EditorResult::ok && enter(Field::password, u"Replacement-only-secret") == EditorResult::ok);
    assert(editor.Save(stop) == EditorResult::ok && ReadConfiguration(file).shares[0].password == "Replacement-only-secret");
    assert(editor.Begin(id) == EditorResult::ok && editor.Password(PasswordChoice::clear) == EditorResult::ok);
    assert(editor.Save(stop) == EditorResult::ok && ReadConfiguration(file).shares[0].password.empty());
    assert(editor.Begin(id) == EditorResult::ok && enter(Field::name, u"Unsaved") == EditorResult::ok);
    const auto before_cancel = Bytes(file); editor.Cancel(); assert(Bytes(file) == before_cancel);
    stop = true; assert(editor.Remove(id, stop) == EditorResult::cancelled && Bytes(file) == before_cancel); stop = false;
    assert(editor.Begin("") == EditorResult::ok);
    assert(enter(Field::server, u"127.0.0.1") == EditorResult::ok && enter(Field::share, u"games") == EditorResult::ok);
    assert(enter(Field::folder, u"roms") == EditorResult::ok && editor.Save(stop) == EditorResult::duplicate);
    editor.Cancel();
    // All explicit NFS options, protocol switching, and four-share admission.
    assert(editor.Begin("") == EditorResult::ok && editor.Change(Field::protocol) == EditorResult::ok);
    assert(enter(Field::user, u"not-applicable") == EditorResult::no_edit);
    assert(enter(Field::server, u"127.0.0.1") == EditorResult::ok && enter(Field::share, u"/games") == EditorResult::ok);
    assert(enter(Field::uid, u"1001") == EditorResult::ok && enter(Field::gid, u"1002") == EditorResult::ok);
    assert(enter(Field::mount_port, u"20048") == EditorResult::ok && enter(Field::port, u"2049") == EditorResult::ok);
    assert(enter(Field::timeout, u"15000") == EditorResult::ok && editor.Save(stop) == EditorResult::ok);
    config = ReadConfiguration(file); assert(config.shares[1].uid == 1001 && config.shares[1].gid == 1002 && config.shares[1].mount_port == 20048);
    for (auto name : {u"third", u"fourth"}) {
        assert(editor.Begin("") == EditorResult::ok && enter(Field::server, u"127.0.0.1") == EditorResult::ok);
        assert(enter(Field::share, name) == EditorResult::ok && editor.Save(stop) == EditorResult::ok);
    }
    assert(editor.Begin("") == EditorResult::limit);
    assert(editor.Remove(id, stop) == EditorResult::ok && ReadConfiguration(file).shares.size() == 3);
    // Cancel during the private keyboard discards even an accepted synthetic answer.
    assert(editor.Begin(config.shares[1].id) == EditorResult::ok);
    assert(editor.Enter(Field::name, stop, [&](const auto&, const auto&) { stop = true; return PrivateAnswer{EditorResult::ok, "Late"}; }) == EditorResult::cancelled);
    stop = false; editor.Cancel();
    // Every prepublication cancellation point preserves the old bytes and cleans only its own stage.
    config = ReadConfiguration(file);
    // A failed bounded disk write is not a successful save and does not truncate the old file.
    struct rlimit previous{}; assert(getrlimit(RLIMIT_FSIZE, &previous) == 0);
    auto limited = previous; limited.rlim_cur = 16;
    const auto signal_before = std::signal(SIGXFSZ, SIG_IGN);
    assert(setrlimit(RLIMIT_FSIZE, &limited) == 0);
    assert(WriteConfiguration(file, config, {}, stop) == ConfigError::write);
    assert(setrlimit(RLIMIT_FSIZE, &previous) == 0); std::signal(SIGXFSZ, signal_before);
    assert(Bytes(file) == config.bytes); no_stages();
    for (auto point : {WriteStep::staged, WriteStep::flushed, WriteStep::publish}) {
        const auto old = Bytes(file);
        assert(WriteConfiguration(file, config, {}, stop, [&](auto step) { if (step == point) stop = true; }) == ConfigError::cancelled);
        stop = false; assert(Bytes(file) == old); no_stages();
    }
    // A concurrent JSON import is never blanket-reset or overwritten.
    assert(WriteConfiguration(file, config, {}, stop, [&](auto step) { if (step == WriteStep::flushed) Put(file, "{\"shares\":[]}"); }) == ConfigError::conflict);
    assert(Bytes(file) == "{\"shares\":[]}"); no_stages();
    const auto sibling = root / "config/network-shares.json.tmp-unrelated";
    fs::create_directory(sibling); Put(sibling / "sentinel", "do-not-reuse");
    config = ReadConfiguration(file); assert(WriteConfiguration(file, config, {}, stop) == ConfigError::none);
    assert(Bytes(sibling / "sentinel") == "do-not-reuse"); fs::remove(sibling / "sentinel"); fs::remove(sibling);
    // Strict schema, duplicate keys/identities, unsupported NFS credentials and bounded files.
    for (const auto& invalid : {std::string{"{broken-private-json"}, std::string{"{\"shares\":[],\"shares\":[]}"},
        std::string{"{\"shares\":[],\"unknown\":true}"}, std::string(65537, ' ')}) {
        Put(file, invalid); assert(editor.Open(file) != EditorResult::ok);
        assert(editor.Begin("") != EditorResult::ok && editor.Save(stop) != EditorResult::ok && Bytes(file) == invalid);
    }
    Put(file, "{\"shares\":[]}"); config = ReadConfiguration(file);
    Share nfs; nfs.id = "bad"; nfs.name = "Invalid NFS"; nfs.protocol = "nfs"; nfs.server = "127.0.0.1"; nfs.share = "/games"; nfs.password = "Invalid";
    assert(WriteConfiguration(file, config, {nfs}, stop) == ConfigError::invalid);
    // Reject FIFO, hardlinks, leaf links and every linked ancestor without touching their targets.
    const auto target = root / "sentinel"; Put(target, "sentinel"); fs::remove(file); fs::create_symlink(target, file);
    assert(editor.Open(file) == EditorResult::unsafe && Bytes(target) == "sentinel"); fs::remove(file);
    fs::create_hard_link(target, file); assert(editor.Open(file) == EditorResult::unsafe); fs::remove(file);
    assert(mkfifo(file.c_str(), 0600) == 0); assert(editor.Open(file) == EditorResult::unsafe); fs::remove(file);
    fs::create_directory_symlink(root / "config", root / "linked");
    assert(ReadConfiguration((root / "linked/network-shares.json").string()).error == ConfigError::unsafe);
    assert(WriteConfiguration((root / "linked/network-shares.json").string(), {}, {}, stop) == ConfigError::unsafe);
    assert(Bytes(target) == "sentinel");
    Put(file, "{\"shares\":[]}"); config = ReadConfiguration(file);
    assert(WriteConfiguration(file, config, {}, stop, [&](auto step) {
        if (step == WriteStep::flushed) { fs::rename(root / "config", root / "moved"); fs::create_directory_symlink(root / "moved", root / "config"); }
    }) == ConfigError::conflict);
    assert(Bytes(root / "moved/network-shares.json") == "{\"shares\":[]}");
    std::cout << "private native keyboard seam; SMB/NFS add/edit/remove; keep/replace/clear; four-share/duplicate/strict JSON bounds; atomic 0600 save, cancellation/conflict/old preservation; no-follow FIFO/link/ancestor rejection PASS\n";
}
