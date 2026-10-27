// SPDX-License-Identifier: GPL-3.0-or-later
#include "pe/ui/scene/package.hpp"
#include "pe/ui/scene/player.hpp"
#include <iostream>

int main(int argc, char** argv) {
    try {
        if (argc != 2) { std::cerr << "Usage: scene-check <pack-directory>\n"; return 2; }
        const auto package = pe::ui::scene::load(argv[1]);
        std::cout << package.model.id << ": " << package.model.screens.size() << " validated screens, "
                  << package.images.size() << " images, " << package.audio.size() << " recordings\n";
        return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
