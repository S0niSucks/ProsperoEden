// SPDX-License-Identifier: GPL-3.0-or-later
#include "file_amiibo.h"
#include "amiibo_file.h"
#include "quick_menu.h"
#include "hud.h"
#include "input_common/input_poller.h"
#include <atomic>
#include <cassert>
#include <cstdio>
#include <fstream>
#include <thread>
#include <unistd.h>

int main() {
    namespace fs = std::filesystem;
    using Nfc = Common::Input::NfcState;
    using Poll = Common::Input::PollingMode;
    using Driver = Eden::FileAmiibo::Driver;
    const fs::path root = fs::temp_directory_path() / ("eden-amiibo-" + std::to_string(getpid()));
    fs::create_directories(root);
    for (const auto size : {0x214, 0x21c, 0x23c, 0x10000}) {
        const auto path = root / (std::to_string(size) + ".bin");
        const std::vector<char> bytes(size, 7);
        std::ofstream(path, std::ios::binary).write(bytes.data(), bytes.size());
    }
    const std::string path = (root / "540.bin").string();
    fs::create_symlink(path, root / "alias.bin");
    fs::copy_file(path, root / ".hidden.bin");
    fs::create_directories(root / "folder.bin");
    assert(mkfifo((root / "pipe.bin").c_str(), 0600) == 0);
    assert(!Eden::FileAmiibo::ValidFile((root / "pipe.bin").string())); // cannot block the menu
    assert(!Eden::AmiiboFile((root / "alias.bin").string()).IsOpen());
    assert(!Eden::AmiiboFile((root / "65536.bin").string()).IsOpen());
    assert(Eden::AmiiboFiles(root.string()).size() == 3);
    auto engine = std::make_shared<Eden::FileAmiibo>();
    InputCommon::InputFactory inputs(engine);
    InputCommon::OutputFactory outputs(engine);
    auto input = inputs.Create(Common::ParamPackage{"engine:virtual_amiibo,nfc:1"});
    auto output = outputs.Create(Common::ParamPackage{"engine:virtual_amiibo,output:1"});
    assert(input && output);
    std::atomic<unsigned> arrived{}, removed{};
    // The same NFC notifications that the core's emulated controller receives.
    input->SetCallback({[&](const Common::Input::CallbackStatus& status) {
        if (status.nfc_status.state == Nfc::NewAmiibo) ++arrived;
        if (status.nfc_status.state == Nfc::AmiiboRemoved) ++removed;
    }});
    assert(engine->Load(path) == Driver::Info::WrongDeviceState);
    assert(output->SetPollingMode(Poll::NFC) == Common::Input::DriverResult::Success);
    assert(output->StartNfcPolling() == Nfc::Success);
    assert(engine->Load((root / "65536.bin").string()) == Driver::Info::NotAnAmiibo);
    assert(engine->Load((root / "alias.bin").string()) == Driver::Info::NotAnAmiibo);
    for (const auto& file : Eden::AmiiboFiles(root.string())) {
        assert(engine->Load(file) == Driver::Info::Success);
        std::vector<u8> data;
        assert(output->ReadAmiiboData(data) == Nfc::Success && data.size() == 0x21c && data[0] == 7);
        if (file.ends_with("532.bin")) assert(data.back() == 0); // missing password is padded
        if (file.ends_with("572.bin")) assert(data.back() == 7); // signature is not copied past 540
        assert(engine->GetNfc({}).uuid_length == 7);
        assert(output->WriteNfcData(std::vector<u8>(0x21c, 19)) == Nfc::Success);
        assert(fs::file_size(file) == (file.ends_with("572.bin") ? 0x23c : 0x21c));
        if (file.ends_with("572.bin")) {
            std::ifstream signed_dump(file, std::ios::binary);
            signed_dump.seekg(0x21c); assert(signed_dump.get() == 7); // preserve signature bytes
        }
        engine->Remove();
        assert(output->ReadAmiiboData(data) == Nfc::WrongDeviceState);
    }
    assert(arrived == 3 && removed == 3);
    assert(engine->Load(path) == Driver::Info::Success);
    std::vector<u8> changed(0x21c, 19);
    assert(output->WriteNfcData(changed) == Nfc::Success);
    std::ifstream saved(path, std::ios::binary);
    assert(saved.get() == 19);
    assert(output->WriteNfcData(std::vector<u8>(0x10000)) == Nfc::WriteFailed);
    // The game's write must not follow a link substituted after the original scan.
    const auto original = root / "original.bin";
    Eden::AmiiboFile opened(path);
    fs::rename(path, original);
    fs::create_symlink(original, path);
    std::array<unsigned char, 0x21c> held{};
    assert(opened.Read(held) == held.size() && held[0] == 19); // reads the verified descriptor
    assert(output->WriteNfcData(std::vector<u8>(0x21c, 42)) == Nfc::WriteFailed);
    std::ifstream untouched(original, std::ios::binary); assert(untouched.get() == 19);
    fs::remove(path); fs::rename(original, path);
    engine->Remove();
    // Load/remove and guest read/poll operations must not race on Eden's state or vectors.
    std::jthread reader([&] {
        for (unsigned n = 0; n < 1000; ++n) {
            std::vector<u8> data;
            const auto result = output->ReadAmiiboData(data);
            assert(result == Nfc::Success || result == Nfc::WrongDeviceState);
            (void)engine->State();
        }
    });
    for (unsigned n = 0; n < 100; ++n) {
        assert(engine->Load(path) == Driver::Info::Success); engine->Remove();
    }
    reader.join();
    {
        using namespace ps5::pad;
        Eden::QuickMenu menu(*engine, root.string());
        menu.Toggle(); assert(menu.Open());
        assert(engine->State() == Driver::State::WaitingForAmiibo); // copying/opening never scans
        const auto first = Eden::QuickOverlay::Text();
        assert(std::string(first.data()).find("1/3") != std::string::npos);
        menu.Press(kButtonRight);
        assert(engine->State() == Driver::State::WaitingForAmiibo); // selecting never scans either
        assert(std::string(Eden::QuickOverlay::Text().data()).find("2/3") != std::string::npos);
        menu.Press(kButtonCross);
        assert(engine->State() == Driver::State::TagNearby);
        assert(std::string(Eden::QuickOverlay::Text().data()).find("TAG PRESENTED") != std::string::npos);
        const auto before_swap = arrived.load();
        menu.Press(kButtonRight); menu.Press(kButtonCross);
        assert(arrived == before_swap); // cannot silently replace the tag the game still sees
        assert(std::string(Eden::QuickOverlay::Text().data()).find("REMOVE THE CURRENT TAG") != std::string::npos);
        menu.Press(kButtonSquare);
        assert(engine->State() == Driver::State::WaitingForAmiibo);
        menu.Press(kButtonCross);
        assert(engine->State() == Driver::State::TagNearby && arrived == before_swap + 1);
        menu.Press(kButtonSquare);
        const auto snapshot = Eden::MakeQuickHudSnapshot(Eden::QuickOverlay::Text().data());
        assert(snapshot.loading & Eden::kHudOverlayFlag);
        assert(snapshot.width > 0 && snapshot.width + 2 * Eden::kHudBoxX <= Eden::kHudPictureWidth);
        menu.Press(kButtonCircle); assert(!menu.Open()); assert(!Eden::QuickOverlay::Text()[0]);
        Eden::QuickMenu missing(*engine, (root / "missing").string());
        missing.Toggle(); missing.Press(kButtonCross);
        assert(std::string(Eden::QuickOverlay::Text().data()).find("COPY .BIN") != std::string::npos);
    }
    assert(output->StopNfcPolling() == Nfc::Success);
    {
        Eden::QuickMenu menu(*engine, root.string());
        menu.Toggle(); menu.Press(ps5::pad::kButtonCross);
        assert(std::string(Eden::QuickOverlay::Text().data()).find("SCAN FIRST") != std::string::npos);
    }
    (void)output->SetPollingMode(Poll::Passive);
    assert(engine->State() == Driver::State::Disabled);
    fs::remove_all(root);
    std::puts("File Amiibo PASS: actual Eden driver and NFC input/output factories, bounded dumps, "
              "tag arrival/removal/UUID/read/write, invalid files and concurrent guest reads");
}
