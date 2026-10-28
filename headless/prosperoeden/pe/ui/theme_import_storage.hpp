// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
namespace pe::ui {
// Called on the install worker (startup recovery: constructing thread). Return a live
// reservation token, or null/throw to deny. Token destruction releases admission credit.
// The caller must outlive RuntimeLauncher and its joined workers. No UI calls in this hook.
using ThemeImportStorage = std::function<std::shared_ptr<void>(const std::string& root,std::uint64_t bytes)>;
}
