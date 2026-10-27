// SPDX-License-Identifier: GPL-3.0-or-later
// Tests the actual catalog Parse/Read/Find source section, without building/linking the NFC core.
#include "amiibo_catalog.h"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <cstdio>
#include <unistd.h>
int main() {
    namespace fs = std::filesystem;
    char temporary[] = "/tmp/eden-amiibo-merge-XXXXXX"; assert(mkdtemp(temporary));
    const fs::path root = temporary;
    const auto write = [&](const char* name, const char* bytes) { std::ofstream(root / name) << bytes; };
    write("catalog-sync.json", R"({"amiibo":[{"head":"00000000","tail":"00000001","name":"Remote name"},{"head":"00000000","tail":"00000002","name":"Remote second"}]})");
    auto result = Eden::AmiiboCatalog::Read(root.string());
    assert(result.Size() == 2 && result.Find("0000000000000001")->name == "Remote name");
    write("amiibo.json", R"({"amiibos":{"0000000000000001":{"name":"Custom name"},"0000000000000003":{"name":"Custom-only model"}}})");
    result = Eden::AmiiboCatalog::Read(root.string());
    assert(result.Size() == 3 && result.Find("0000000000000001")->name == "Custom name");
    assert(result.Find("0000000000000002")->name == "Remote second");
    assert(result.Find("0000000000000003")->name == "Custom-only model");
    write("catalog-sync.json", "invalid network cache");
    result = Eden::AmiiboCatalog::Read(root.string());
    assert(result.Size() == 2 && result.Find("0000000000000001")->name == "Custom name");
    fs::remove(root / "catalog-sync.json");
    fs::create_symlink(root / "amiibo.json", root / "catalog-sync.json");
    result = Eden::AmiiboCatalog::Read(root.string());
    assert(result.Size() == 2 && result.Find("0000000000000001")->name == "Custom name");
    fs::remove_all(root);
    std::puts("Amiibo merged reader PASS: actual Parse/Read/Find source, downloaded fallback, custom names/IDs win, invalid/symlink downloaded cache preserves local metadata (NFC core not linked)");
}
