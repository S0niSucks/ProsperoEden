// SPDX-License-Identifier: GPL-3.0-or-later
#include "live_cheats.h"
#include "cheat_catalog.h"
#include "quick_menu.h"
#include "core/memory/cheat_engine.h"
#include <cassert>
#include <cstring>
#include <cstdio>
#include <thread>

namespace {
struct Memory final : Core::Memory::DmntCheatVm::Callbacks {
    std::array<u8, 16> bytes{};
    unsigned writes = 0;
    void MemoryReadUnsafe(VAddr address, void* out, u64 size) override {
        assert(address >= 0x1000 && address + size <= 0x1010);
        std::memcpy(out, bytes.data() + address - 0x1000, size);
    }
    void MemoryWriteUnsafe(VAddr address, const void* in, u64 size) override {
        assert(address >= 0x1000 && address + size <= 0x1010);
        std::memcpy(bytes.data() + address - 0x1000, in, size); ++writes;
    }
    u64 HidKeysDown() override { return 0; }
    void PauseProcess() override {}
    void ResumeProcess() override {}
    void DebugLog(u8, u64) override {}
    void CommandLog(std::string_view) override {}
};
}
int main() {
    namespace Live = Eden::LiveCheats;
    using namespace ps5::pad;
    std::array<u8, 32> build{}; build[0] = 0x77;
    Core::Memory::TextCheatParser parser;
    // A real, harmless VM write into the test callback's own 16-byte window.
    const std::string database = R"({"0100000000001000":{"7700000000000000":{")" + std::string(64, 'a') +
        R"(":{"title":"[Write value]","source":"04000000 00000000 00000001"},")" + std::string(64, 'b') +
        R"(":{"title":"{Master}","source":"04000000 00000004 00000002"},"version":0}}})";
    const auto catalog = Eden::ReadCheatCatalog(database, 0x0100000000001000, build);
    assert(catalog.status == Eden::CheatCatalogStatus::Ready);
    auto list = parser.Parse(catalog.Text());
    assert(list.size() == 2 && list[0].cheat_id == 0 && list[1].enabled);
    const int engine = 1, other = 2;
    Live::Begin(&engine, list, build);
    auto view = Live::Read(); assert(view.build == build && view.rows.size() == 1);
    assert(view.rows[0].name == "Write value" && view.rows[0].enabled);
    assert(!Live::Toggle(view.generation, 0)); // master never appears as a switch
    auto memory = std::make_unique<Memory>(); auto& window = *memory;
    Core::Memory::DmntCheatVm vm(std::move(memory));
    Core::Memory::CheatProcessMetadata metadata{}; metadata.main_nso_extents = {0x1000, 16};
    assert(vm.LoadProgram(list)); vm.Execute(metadata);
    assert(window.bytes[0] == 1 && window.bytes[4] == 2 && window.writes == 2);
    Eden::FileAmiibo amiibo;
    Eden::QuickMenu menu(amiibo, "/no-amiibo-fixtures");
    menu.Toggle(); menu.Press(kButtonDown); menu.Press(kButtonCross);
    assert(!Live::Read().rows[0].enabled);
    assert(std::string(Eden::QuickOverlay::Text().data()).find("OFF WRITE VALUE") != std::string::npos);
    auto reload = Live::Take(&engine); assert(reload && !(*reload)[1].enabled && (*reload)[0].enabled);
    assert(!Live::Take(&engine)); assert(!Live::Take(&other));
    window.bytes.fill(0); window.writes = 0;
    assert(vm.LoadProgram(*reload)); vm.Execute(metadata);
    assert(window.bytes[0] == 0 && window.bytes[4] == 2 && window.writes == 1);
    menu.Press(kButtonCross); reload = Live::Take(&engine);
    assert(reload && (*reload)[1].enabled);
    window.bytes.fill(0); window.writes = 0;
    assert(vm.LoadProgram(*reload)); vm.Execute(metadata);
    assert(window.bytes[0] == 1 && window.writes == 2);
    // A new engine invalidates all UI handles, even if the new row has the same name/index.
    Live::Begin(&other, list, build);
    assert(!Live::Toggle(view.generation, 1)); Live::End(&engine);
    assert(Live::Read().rows.size() == 1);
    view = Live::Read();
    std::jthread producer([&] {
        for (unsigned n = 0; n < 10000; ++n) assert(Live::Toggle(view.generation, 1));
    });
    for (unsigned n = 0; n < 10000; ++n) {
        const auto pending = Live::Take(&other);
        if (pending) assert(pending->size() == 2 && (*pending)[0].enabled);
        assert(Live::Read().rows.size() == 1);
    }
    producer.join();
    Live::Replace(&other, list); assert(!Live::Toggle(view.generation, 1));
    assert(Live::Take(&other));
    Live::End(&other); assert(Live::Read().rows.empty()); assert(!Live::Take(&other));
    menu.Press(kButtonCircle); assert(!Eden::QuickOverlay::Text()[0]);
    std::puts("Live cheats PASS: actual parser/VM writes on/off/on via quick menu; masters retained, "
              "session/build binding, stale handles and 10000 concurrent updates");
}
