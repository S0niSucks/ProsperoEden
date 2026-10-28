// SPDX-License-Identifier: GPL-3.0-or-later
#include "shares.h"
#include <cassert>
#include <chrono>
#include <iostream>
#include <thread>
int main(int argc, char** argv) {
    assert(argc == 3);
    using namespace Eden::Network;
    Configure(argv[1]);
    for (unsigned n = 0; n < 400 && Current().busy; ++n)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    const bool expected = std::string_view{argv[2]} == "yes";
    assert(!Current().busy && Current().shares.size() == 1);
    assert(Current().shares[0].connected == expected);
    if (expected) {
        auto f = Open("pe-net://test/Proof.nro"); assert(f.session);
        unsigned char bytes[8]{};
        assert(f.session->Read(f.entry, bytes, sizeof(bytes), 3) == sizeof(bytes));
        assert((std::string_view{reinterpret_cast<char*>(bytes), 8} == "3456789a"));
    } else {
        assert(!Current().shares[0].error.empty());
        assert(!Known("pe-net://test/Proof.nro"));
    }
    Disconnect();
    std::cout << "PASS\n";
}
