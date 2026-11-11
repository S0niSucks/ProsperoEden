// SPDX-License-Identifier: GPL-3.0-or-later
// Host check of the P09 restore barrier (usb_restore_barrier.h), r2 with the
// A21 review regressions. Everything runs on synthetic directories under a
// private mkdtemp tree: no real user data, no USB device, no production root.
//
// The fixture computes its own manifest checksums with an independent FNV-1a
// oracle so the header's implementation is checked against a second encoding.
//
// Build (centralized runs only; matches the eden-settings-check pattern):
//   c++ -std=c++20 -UNDEBUG -I<headless dir> usb_restore_barrier_check.cpp -o check && ./check
#include "usb_restore_barrier.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include <unistd.h>

namespace fs = std::filesystem;
using Eden::Restore::Entry;
using Eden::Restore::Faults;
using Eden::Restore::InspectPackage;
using Eden::Restore::InspectPending;
using Eden::Restore::Options;
using Eden::Restore::PackageInfo;
using Eden::Restore::PendingState;
using Eden::Restore::Reason;
using Eden::Restore::Result;

#define CHECK(condition)                                                                        \
    do {                                                                                        \
        if (!(condition)) {                                                                     \
            std::fprintf(stderr, "EDEN_RESTORE_CHECK FAILED line=%d condition=%s\n", __LINE__,  \
                         #condition);                                                           \
            return 1;                                                                           \
        }                                                                                       \
    } while (0)

// r3.1/A50: CHECK returns `int` and is only valid in the int-valued test cases.
// Helpers that return Layout/std::map cannot `return 1`; a failed precondition
// there aborts the whole run with the same loud FAILED line instead.
#define CHECK_ABORT(condition)                                                                  \
    do {                                                                                        \
        if (!(condition)) {                                                                     \
            std::fprintf(stderr, "EDEN_RESTORE_CHECK FAILED line=%d condition=%s\n", __LINE__,  \
                         #condition);                                                           \
            std::exit(1);                                                                       \
        }                                                                                       \
    } while (0)

static std::uint64_t OracleFnv(const std::string& bytes) {
    std::uint64_t state = 14695981039346656037ULL;
    for (unsigned char c : bytes) {
        state ^= c;
        state *= 1099511628211ULL;
    }
    return state;
}

static void WriteBytes(const std::string& path, const std::string& bytes) {
    fs::create_directories(fs::path(path).parent_path());
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
}

static std::string ReadBytes(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), {}};
}

struct Layout {
    std::string root, data, tx, pkg;
    Options OptionsFor() const { return Options{data, tx}; }
};

static Layout MakeLayout() {
    char directory[] = "/tmp/eden-restore-barrier-XXXXXX";
    CHECK_ABORT(mkdtemp(directory));
    Layout layout;
    layout.root = directory;
    layout.data = layout.root + "/data";
    layout.tx = layout.data + "/.restore-tx";
    layout.pkg = layout.root + "/pkg";
    CHECK_ABORT(fs::create_directories(layout.data));
    CHECK_ABORT(fs::create_directories(layout.pkg));
    return layout;
}

// Builds a package whose manifest is internally consistent; returns its digest.
static std::uint64_t BuildPackage(const std::string& pkg,
                                  const std::vector<std::pair<std::string, std::string>>& files) {
    std::vector<std::pair<std::string, std::string>> sorted = files;
    bool sorted_ok = true;
    do {
        sorted_ok = true;
        for (std::size_t i = 1; i < sorted.size(); ++i)
            if (sorted[i - 1].first > sorted[i].first) {
                std::swap(sorted[i - 1], sorted[i]);
                sorted_ok = false;
            }
    } while (!sorted_ok);
    std::string manifest = "EDEN-RESTORE-1\n" + std::to_string(sorted.size()) + "\n";
    for (const auto& [path, content] : sorted) {
        WriteBytes(pkg + "/" + path, content);
        char line[512];
        std::snprintf(line, sizeof(line), "%016llx %zu %s\n",
                      static_cast<unsigned long long>(OracleFnv(content)), content.size(), path.c_str());
        manifest += line;
    }
    WriteBytes(pkg + "/" + Eden::Restore::kManifestName, manifest);
    return OracleFnv(manifest);
}

// data-root contents excluding the transaction subtree.
static std::map<std::string, std::string> SnapshotData(const Layout& layout) {
    std::map<std::string, std::string> snapshot;
    for (const auto& entry : fs::recursive_directory_iterator(layout.data)) {
        const std::string relative = fs::relative(entry.path(), layout.data).generic_string();
        if (relative == ".restore-tx" || relative.rfind(".restore-tx/", 0) == 0) continue;
        CHECK_ABORT(entry.is_regular_file());
        snapshot[relative] = ReadBytes(entry.path().generic_string());
    }
    return snapshot;
}

// Flags only OUR temp pattern (<base>.restore-new.<pid>.<seq>); a user file
// named exactly "<base>.restore-new" is legitimate content, not residue (M1).
static bool NoStrayTempFiles(const Layout& layout) {
    for (const auto& entry : fs::recursive_directory_iterator(layout.data)) {
        const std::string name = entry.path().filename().generic_string();
        if (name.find(".restore-new.") != std::string::npos ||
            name.find(".restore-rollback.") != std::string::npos)
            return false;
    }
    return true;
}

static bool ReasonIs(const Result& result, Reason reason) {
    if (result.ok || result.reason != reason) {
        std::fprintf(stderr, "EDEN_RESTORE_CHECK expected reason=%s got=%s detail=%s\n",
                     Eden::Restore::ToString(reason), Eden::Restore::ToString(result.reason),
                     result.detail.c_str());
        return false;
    }
    return true;
}

using Eden::Restore::ApplyPending;
using Eden::Restore::CancelPending;
using Eden::Restore::StageRestore;

// TC1: valid package stages, applies on the simulated next startup, and leaves
// no transaction state behind. Covers replaced and created files, nesting and
// zero-length payloads.
static int ValidApply() {
    Layout l = MakeLayout();
    WriteBytes(l.data + "/a.txt", "old-a");
    WriteBytes(l.data + "/b.bin", std::string{"\0\x1\x2old-b", 9});
    const std::vector<std::pair<std::string, std::string>> files = {
        {"a.txt", "new-a"},
        {"b.bin", std::string{"\xff\xfe new-b", 8}},
        {"empty.dat", ""},
        {"sub/deep/c.txt", "nested-c"},
        {"sub/new.txt", "new-in-sub"},
    };
    const std::uint64_t digest = BuildPackage(l.pkg, files);
    PackageInfo info;
    CHECK(InspectPackage(l.pkg, &info));
    CHECK(info.manifest_checksum == digest);
    CHECK(info.entries.size() == files.size());

    CHECK(StageRestore(l.OptionsFor(), l.pkg, digest));
    const PendingState pending = InspectPending(l.OptionsFor());
    CHECK(pending.active && pending.readable && pending.manifest_checksum == digest);
    CHECK(SnapshotData(l).at("a.txt") == "old-a"); // staging never touches data

    CHECK(ApplyPending(l.OptionsFor())); // simulated next startup
    for (const auto& [path, content] : files)
        CHECK(ReadBytes(l.data + "/" + path) == content);
    CHECK(!InspectPending(l.OptionsFor()).active);
    CHECK(!fs::exists(l.tx + "/staged"));
    CHECK(!fs::exists(l.tx + "/recovery"));
    CHECK(!fs::exists(l.tx + "/completed"));
    CHECK(NoStrayTempFiles(l));
    fs::remove_all(l.root);
    std::puts("EDEN_RESTORE_CHECK tc=valid-apply ok");
    return 0;
}

// TC2: truncated package is refused at staging; nothing is pending, data keeps
// its previous state.
static int TruncatedRefusedBeforeStaging() {
    Layout l = MakeLayout();
    WriteBytes(l.data + "/a.txt", "old-a");
    const std::uint64_t digest = BuildPackage(l.pkg, {{"a.txt", "new-a"}, {"z.txt", "new-z"}});
    std::error_code error;
    fs::resize_file(l.pkg + "/a.txt", 2, error); // truncated payload
    CHECK(!error);
    const Result staged = StageRestore(l.OptionsFor(), l.pkg, digest);
    CHECK(ReasonIs(staged, Reason::SizeMismatch));
    CHECK(!InspectPending(l.OptionsFor()).active);
    CHECK(SnapshotData(l).at("a.txt") == "old-a");
    fs::remove_all(l.root);
    std::puts("EDEN_RESTORE_CHECK tc=truncated-refused-before-staging ok");
    return 0;
}

// TC3: staged package corrupted afterwards (drive pulled/changed): apply
// refuses with nothing applied and the pending barrier stays on.
static int CorruptedStagedRefused() {
    Layout l = MakeLayout();
    WriteBytes(l.data + "/a.txt", "old-a");
    const std::uint64_t digest = BuildPackage(l.pkg, {{"a.txt", "new-a"}, {"z.txt", "new-z"}});
    CHECK(StageRestore(l.OptionsFor(), l.pkg, digest));
    const std::string staged_file = l.tx + "/staged/a.txt";
    std::string bytes = ReadBytes(staged_file);
    bytes[0] = bytes[0] == 'X' ? 'Y' : 'X';
    WriteBytes(staged_file, bytes);
    CHECK(ReasonIs(ApplyPending(l.OptionsFor()), Reason::ValidationRefused));
    CHECK(SnapshotData(l).at("a.txt") == "old-a");
    CHECK(!SnapshotData(l).count("z.txt"));
    CHECK(InspectPending(l.OptionsFor()).active);
    CHECK(fs::exists(l.tx + "/staged")); // kept for re-inspection
    fs::remove_all(l.root);
    std::puts("EDEN_RESTORE_CHECK tc=corrupted-staged-refused ok");
    return 0;
}

// TC4: failure in the middle of the apply restores the exact previous state,
// keeps the pending barrier, and a later clean startup applies successfully.
static int MidFailureKeepsPreviousState() {
    Layout l = MakeLayout();
    WriteBytes(l.data + "/a.txt", "old-a");
    WriteBytes(l.data + "/keep.txt", "keep-me");
    const std::uint64_t digest =
        BuildPackage(l.pkg, {{"a.txt", "new-a"}, {"sub/new.txt", "new-in-sub"}, {"z.txt", "new-z"}});
    CHECK(StageRestore(l.OptionsFor(), l.pkg, digest));
    const std::map<std::string, std::string> before = SnapshotData(l);
    Faults faults;
    faults.fail_apply_after = 0; // fail after the first install
    CHECK(ReasonIs(ApplyPending(l.OptionsFor(), &faults), Reason::ApplyFailed));
    CHECK(SnapshotData(l) == before); // exact previous state, no partial tree
    CHECK(NoStrayTempFiles(l));
    CHECK(InspectPending(l.OptionsFor()).active);
    CHECK(ApplyPending(l.OptionsFor())); // retry on a later startup succeeds
    CHECK(ReadBytes(l.data + "/a.txt") == "new-a");
    CHECK(ReadBytes(l.data + "/sub/new.txt") == "new-in-sub");
    CHECK(!InspectPending(l.OptionsFor()).active);
    fs::remove_all(l.root);
    std::puts("EDEN_RESTORE_CHECK tc=mid-failure-previous-state ok");
    return 0;
}

// TC5: rollback itself failing is fail-closed: inconsistent marker, barrier on,
// later applies/cancels refuse, recovery data retained for manual repair.
static int RollbackFailureFailsClosed() {
    Layout l = MakeLayout();
    WriteBytes(l.data + "/a.txt", "old-a");
    const std::uint64_t digest = BuildPackage(l.pkg, {{"a.txt", "new-a"}, {"z.txt", "new-z"}});
    CHECK(StageRestore(l.OptionsFor(), l.pkg, digest));
    Faults faults;
    faults.fail_apply_after = 0;
    faults.fail_rollback_after = 0;
    CHECK(ReasonIs(ApplyPending(l.OptionsFor(), &faults), Reason::RollbackFailed));
    CHECK(fs::exists(l.tx + "/inconsistent"));
    CHECK(InspectPending(l.OptionsFor()).active);
    CHECK(ReasonIs(ApplyPending(l.OptionsFor()), Reason::InconsistentState));
    CHECK(ReasonIs(CancelPending(l.OptionsFor()), Reason::InconsistentState));
    CHECK(!fs::exists(l.tx + "/completed"));
    fs::remove_all(l.root);
    std::puts("EDEN_RESTORE_CHECK tc=rollback-failure-fails-closed ok");
    return 0;
}

// TC6: confirmation is bound to the inspected package digest; a changed
// (internally consistent) package is refused as stale, with no barrier created.
static int StaleConfirmationRefused() {
    Layout l = MakeLayout();
    const std::uint64_t digest = BuildPackage(l.pkg, {{"a.txt", "new-a"}});
    PackageInfo info;
    CHECK(InspectPackage(l.pkg, &info));
    const std::uint64_t changed = BuildPackage(l.pkg, {{"a.txt", "changed-a"}});
    CHECK(changed != digest);
    CHECK(ReasonIs(StageRestore(l.OptionsFor(), l.pkg, digest), Reason::StaleConfirmation));
    CHECK(!InspectPending(l.OptionsFor()).active);
    CHECK(StageRestore(l.OptionsFor(), l.pkg, changed)); // confirm the new digest works
    CHECK(CancelPending(l.OptionsFor()));
    fs::remove_all(l.root);
    std::puts("EDEN_RESTORE_CHECK tc=stale-confirmation-refused ok");
    return 0;
}

// TC7: manifest schema enforcement — unsafe paths, ordering, duplicates and
// structure are refused before any payload access matters.
static int ManifestSchemaRefused() {
    Layout l = MakeLayout();
    WriteBytes(l.pkg + "/a.txt", "a");
    WriteBytes(l.pkg + "/b.txt", "b");
    // r3.2/A50: CHECK (`return 1;`) is invalid inside this void-returning
    // lambda (deduces a non-void return type -> -Werror=return-type); the
    // aborting variant is context-safe here.
    const auto expect = [&](const std::string& manifest, Reason reason) {
        WriteBytes(l.pkg + "/" + Eden::Restore::kManifestName, manifest);
        Result inspected = InspectPackage(l.pkg, nullptr);
        CHECK_ABORT(ReasonIs(inspected, reason));
        CHECK_ABORT(ReasonIs(StageRestore(l.OptionsFor(), l.pkg, 0), reason));
        CHECK_ABORT(!InspectPending(l.OptionsFor()).active);
    };
    const std::uint64_t sum_a = [] {
        std::uint64_t h = 14695981039346656037ULL;
        h ^= static_cast<unsigned char>('a');
        h *= 1099511628211ULL;
        return h;
    }();
    const std::uint64_t sum_b = [] {
        std::uint64_t h = 14695981039346656037ULL;
        h ^= static_cast<unsigned char>('b');
        h *= 1099511628211ULL;
        return h;
    }();
    char a_line[64], b_line[64];
    std::snprintf(a_line, sizeof(a_line), "%016llx 1 a.txt", static_cast<unsigned long long>(sum_a));
    std::snprintf(b_line, sizeof(b_line), "%016llx 1 b.txt", static_cast<unsigned long long>(sum_b));
    {
        // Control: the same builder logic parses cleanly before the refusals.
        WriteBytes(l.pkg + "/" + Eden::Restore::kManifestName,
                   "EDEN-RESTORE-1\n2\n" + std::string{a_line} + "\n" + std::string{b_line} + "\n");
        Result inspected = InspectPackage(l.pkg, nullptr);
        CHECK(inspected);
        CHECK(inspected.reason == Reason::Ok);
    }
    expect("EDEN-RESTORE-1\n1\n000000000000000g 1 a.txt\n", Reason::InvalidPackage); // bad hex field
    expect("EDEN-RESTORE-1\n1\n0000000000000000 1 /etc/passwd\n", Reason::UnsafePath);
    expect("EDEN-RESTORE-1\n1\n0000000000000000 1 ../up.txt\n", Reason::UnsafePath);
    expect("EDEN-RESTORE-1\n1\n0000000000000000 1 .hidden/f.txt\n", Reason::UnsafePath);
    expect("EDEN-RESTORE-1\n1\n0000000000000000 1 a\\b.txt\n", Reason::UnsafePath);
    expect("EDEN-RESTORE-1\n1\n0000000000000000 1 a/../b.txt\n", Reason::UnsafePath);
    expect("EDEN-RESTORE-1\n2\n" + std::string{b_line} + "\n" + std::string{a_line} + "\n",
           Reason::UnsortedPaths); // out of order
    expect("EDEN-RESTORE-1\n2\n" + std::string{a_line} + "\n" + std::string{a_line} + "\n",
           Reason::UnsortedPaths); // duplicate
    expect("EDEN-RESTORE-1\n0\n", Reason::InvalidPackage);
    expect("EDEN-RESTORE-1\n1\n" + std::string{a_line} + "\nextra\n", Reason::InvalidPackage);
    expect("EDEN-RESTORE-1\n1\n" + std::string{a_line}, Reason::InvalidPackage); // no final newline
    expect("EDEN-RESTORE-9\n1\n" + std::string{a_line} + "\n", Reason::InvalidPackage);
    fs::remove_all(l.root);
    std::puts("EDEN_RESTORE_CHECK tc=manifest-schema-refused ok");
    return 0;
}

// TC8: links anywhere on the package path are refused.
static int LinksRefused() {
    Layout l = MakeLayout();
    WriteBytes(l.pkg + "/real.txt", "real");
    fs::create_directories(l.pkg + "/dir");
    WriteBytes(l.pkg + "/dir/inside.txt", "inside");
    CHECK(BuildPackage(l.pkg, {{"dir/inside.txt", "inside"}}));
    std::error_code error;
    fs::create_symlink("real.txt", l.pkg + "/real-link.txt", error);
    CHECK(!error);
    fs::create_symlink("dir", l.pkg + "/dir-link", error);
    CHECK(!error);
    const std::uint64_t sum_real = OracleFnv("real");
    const std::uint64_t sum_inside = OracleFnv("inside");
    char inside_line[80], link_line[80];
    // Leaf symlink listed in the manifest.
    std::snprintf(inside_line, sizeof(inside_line), "%016llx 6 dir/inside.txt",
                  static_cast<unsigned long long>(sum_inside));
    std::snprintf(link_line, sizeof(link_line), "%016llx 4 real-link.txt",
                  static_cast<unsigned long long>(sum_real));
    WriteBytes(l.pkg + "/" + Eden::Restore::kManifestName,
               "EDEN-RESTORE-1\n2\n" + std::string{inside_line} + "\n" + std::string{link_line} + "\n");
    CHECK(ReasonIs(InspectPackage(l.pkg, nullptr), Reason::NotRegularFile));
    // Symlinked ancestor directory.
    std::snprintf(link_line, sizeof(link_line), "%016llx 6 dir-link/inside.txt",
                  static_cast<unsigned long long>(sum_inside));
    WriteBytes(l.pkg + "/" + Eden::Restore::kManifestName,
               "EDEN-RESTORE-1\n1\n" + std::string{link_line} + "\n");
    CHECK(ReasonIs(InspectPackage(l.pkg, nullptr), Reason::LinkInPath));
    fs::remove_all(l.root);
    std::puts("EDEN_RESTORE_CHECK tc=links-refused ok");
    return 0;
}

// TC9: explicit cancel removes the barrier without touching data; an unreadable
// pending record keeps failing closed and is never silently discarded.
static int CancelAndUnreadablePending() {
    Layout l = MakeLayout();
    WriteBytes(l.data + "/a.txt", "old-a");
    const std::uint64_t digest = BuildPackage(l.pkg, {{"a.txt", "new-a"}});
    CHECK(StageRestore(l.OptionsFor(), l.pkg, digest));
    const auto before = SnapshotData(l);
    CHECK(CancelPending(l.OptionsFor()));
    CHECK(!InspectPending(l.OptionsFor()).active);
    CHECK(SnapshotData(l) == before);
    CHECK(!fs::exists(l.tx + "/staged"));

    WriteBytes(l.tx + "/pending", "garbage-not-a-record");
    PendingState broken = InspectPending(l.OptionsFor());
    CHECK(broken.active && !broken.readable);
    CHECK(ReasonIs(ApplyPending(l.OptionsFor()), Reason::PendingInvalid));
    CHECK(ReasonIs(CancelPending(l.OptionsFor()), Reason::PendingInvalid));
    CHECK(fs::exists(l.tx + "/pending")); // kept: never silently discarded
    fs::remove_all(l.root);
    std::puts("EDEN_RESTORE_CHECK tc=cancel-unreadable-pending ok");
    return 0;
}

// TC10: an interrupted final cleanup (completed marker present, pending still
// there) resolves to "already applied": cleanup finishes — including the
// completion marker itself (r3/F1) — with no second apply, and the installed
// bytes stand (boot-side half of the r2/B3 contract).
static int CompletedWinsOverStalePending() {
    Layout l = MakeLayout();
    WriteBytes(l.data + "/a.txt", "old-a");
    const std::uint64_t digest = BuildPackage(l.pkg, {{"a.txt", "new-a"}});
    CHECK(StageRestore(l.OptionsFor(), l.pkg, digest));
    CHECK(ApplyPending(l.OptionsFor()));
    CHECK(!fs::exists(l.tx + "/completed")); // success leaves no transaction state (r3/F1)
    // Simulate the crash window: both records still present after completion.
    WriteBytes(l.tx + "/pending", "EDEN-RESTORE-PENDING-1\nmanifest=0000000000000000\nentries=1\nbytes=1\n");
    WriteBytes(l.tx + "/completed", "EDEN-RESTORE-COMPLETED-1\n");
    CHECK(ApplyPending(l.OptionsFor()));
    CHECK(ReadBytes(l.data + "/a.txt") == "new-a"); // installed data stands
    CHECK(!InspectPending(l.OptionsFor()).active);
    CHECK(!fs::exists(l.tx + "/completed"));
    fs::remove_all(l.root);
    std::puts("EDEN_RESTORE_CHECK tc=completed-wins ok");
    return 0;
}

// TC11: no pending restore means apply/cancel report that, changing nothing.
static int NoPendingIsANoOp() {
    Layout l = MakeLayout();
    WriteBytes(l.data + "/a.txt", "old-a");
    CHECK(ReasonIs(ApplyPending(l.OptionsFor()), Reason::NoPending));
    CHECK(ReasonIs(CancelPending(l.OptionsFor()), Reason::NoPending));
    CHECK(ReadBytes(l.data + "/a.txt") == "old-a");
    fs::remove_all(l.root);
    std::puts("EDEN_RESTORE_CHECK tc=no-pending ok");
    return 0;
}

// TC12 (r2/B2 regression): entries that PRE-EXIST after the failure point must
// not break rollback. With lazy recovery (r1) the untouched "m.txt"/"z.txt"
// had no recovery copies and rollback failed into `inconsistent`; now the
// eager phase materializes every copy first and the exact previous state
// returns, followed by a successful clean retry.
static int PreExistingAfterFailurePoint() {
    Layout l = MakeLayout();
    WriteBytes(l.data + "/a.txt", "old-a");
    WriteBytes(l.data + "/m.txt", "old-m");
    WriteBytes(l.data + "/z.txt", "old-z");
    const std::uint64_t digest =
        BuildPackage(l.pkg, {{"a.txt", "new-a"}, {"m.txt", "new-m"}, {"z.txt", "new-z"}});
    CHECK(StageRestore(l.OptionsFor(), l.pkg, digest));
    const std::map<std::string, std::string> before = SnapshotData(l);
    Faults faults;
    faults.fail_apply_after = 0; // a.txt installed; m.txt/z.txt untouched pre-existing
    CHECK(ReasonIs(ApplyPending(l.OptionsFor(), &faults), Reason::ApplyFailed));
    CHECK(SnapshotData(l) == before); // all three originals intact
    CHECK(!fs::exists(l.tx + "/inconsistent"));
    CHECK(InspectPending(l.OptionsFor()).active);
    CHECK(ApplyPending(l.OptionsFor())); // clean retry applies everything
    CHECK(ReadBytes(l.data + "/m.txt") == "new-m");
    CHECK(ReadBytes(l.data + "/z.txt") == "new-z");
    CHECK(!InspectPending(l.OptionsFor()).active);
    CHECK(NoStrayTempFiles(l));
    fs::remove_all(l.root);
    std::puts("EDEN_RESTORE_CHECK tc=pre-existing-after-failure ok");
    return 0;
}

// TC13 (r2/B3 regression): a directory-sync failure AFTER the completed marker
// rename is "published, degraded": the installs stand, the result is Ok with a
// note, and nothing is ever rolled back under the published marker.
static int CompletedSyncDegradedStillApplied() {
    Layout l = MakeLayout();
    WriteBytes(l.data + "/a.txt", "old-a");
    WriteBytes(l.data + "/z.txt", "old-z");
    const std::uint64_t digest = BuildPackage(l.pkg, {{"a.txt", "new-a"}, {"z.txt", "new-z"}});
    CHECK(StageRestore(l.OptionsFor(), l.pkg, digest));
    Faults faults;
    faults.fail_completed_sync = true;
    const Result applied = ApplyPending(l.OptionsFor(), &faults);
    CHECK(applied.ok); // published marker stands; never rolled back
    CHECK(applied.detail.find("sync") != std::string::npos);
    CHECK(ReadBytes(l.data + "/a.txt") == "new-a"); // installs kept
    CHECK(ReadBytes(l.data + "/z.txt") == "new-z");
    CHECK(!InspectPending(l.OptionsFor()).active); // transaction finished
    CHECK(ReasonIs(ApplyPending(l.OptionsFor()), Reason::NoPending));
    CHECK(NoStrayTempFiles(l));
    fs::remove_all(l.root);
    std::puts("EDEN_RESTORE_CHECK tc=completed-sync-degraded ok");
    return 0;
}

// TC14 (r2/M1 regression): a user file whose NAME looks like an old temp
// ("a.restore-new") is legitimate content and must never be unlinked.
static int UserTempLookalikePreserved() {
    Layout l = MakeLayout();
    WriteBytes(l.data + "/a", "old-a");
    WriteBytes(l.data + "/a.restore-new", "precious-user-file");
    const std::uint64_t digest = BuildPackage(l.pkg, {{"a", "new-a"}});
    CHECK(StageRestore(l.OptionsFor(), l.pkg, digest));
    CHECK(ApplyPending(l.OptionsFor()));
    CHECK(ReadBytes(l.data + "/a") == "new-a");
    CHECK(ReadBytes(l.data + "/a.restore-new") == "precious-user-file"); // untouched
    CHECK(NoStrayTempFiles(l));
    fs::remove_all(l.root);
    std::puts("EDEN_RESTORE_CHECK tc=user-temp-lookalike-preserved ok");
    return 0;
}

// TC15 (r2/M2 regression): crashed record-staging residues (.tmp) never block
// staging or the startup apply; plain regular residues are swept, and the
// transaction completes normally.
static int OrphanRecordResiduesTolerated() {
    Layout l = MakeLayout();
    WriteBytes(l.data + "/a.txt", "old-a");
    const std::uint64_t digest = BuildPackage(l.pkg, {{"a.txt", "new-a"}});
    CHECK(StageRestore(l.OptionsFor(), l.pkg, digest));
    CHECK(CancelPending(l.OptionsFor()));
    // Crash residues of record staging (ours by construction).
    WriteBytes(l.tx + "/pending.tmp", "x");
    WriteBytes(l.tx + "/plan.tmp", "x");
    WriteBytes(l.tx + "/completed.tmp", "x");
    CHECK(!InspectPending(l.OptionsFor()).active);
    CHECK(StageRestore(l.OptionsFor(), l.pkg, digest)); // sweep clears pending.tmp
    CHECK(ApplyPending(l.OptionsFor()));                // sweep + EEXIST tolerance clear the rest
    CHECK(ReadBytes(l.data + "/a.txt") == "new-a");
    CHECK(!fs::exists(l.tx + "/plan.tmp"));
    CHECK(!fs::exists(l.tx + "/completed.tmp"));
    CHECK(!InspectPending(l.OptionsFor()).active);
    fs::remove_all(l.root);
    std::puts("EDEN_RESTORE_CHECK tc=orphan-record-residues ok");
    return 0;
}

int main() {
    if (ValidApply()) return 1;
    if (TruncatedRefusedBeforeStaging()) return 1;
    if (CorruptedStagedRefused()) return 1;
    if (MidFailureKeepsPreviousState()) return 1;
    if (RollbackFailureFailsClosed()) return 1;
    if (StaleConfirmationRefused()) return 1;
    if (ManifestSchemaRefused()) return 1;
    if (LinksRefused()) return 1;
    if (CancelAndUnreadablePending()) return 1;
    if (CompletedWinsOverStalePending()) return 1;
    if (NoPendingIsANoOp()) return 1;
    if (PreExistingAfterFailurePoint()) return 1;
    if (CompletedSyncDegradedStillApplied()) return 1;
    if (UserTempLookalikePreserved()) return 1;
    if (OrphanRecordResiduesTolerated()) return 1;
    std::puts("EDEN_RESTORE_CHECK PASS cases=15");
    return 0;
}
