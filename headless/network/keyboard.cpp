// SPDX-License-Identifier: GPL-3.0-or-later
#include "editor.h"
#include "system_keyboard.h"
#include <algorithm>
#include <stdexcept>

namespace Eden::Network {
namespace {
std::u16string To16(const std::string& value) {
    std::u16string out;
    for (std::size_t i = 0; i < value.size();) {
        auto c = static_cast<unsigned char>(value[i++]); unsigned code = c, count = 0, minimum = 0;
        if (c >= 0xc2 && c <= 0xdf) { code = c & 31; count = 1; minimum = 0x80; }
        else if (c >= 0xe0 && c <= 0xef) { code = c & 15; count = 2; minimum = 0x800; }
        else if (c >= 0xf0 && c <= 0xf4) { code = c & 7; count = 3; minimum = 0x10000; }
        else if (c >= 0x80) throw std::runtime_error("encoding");
        while (count > 0) { --count; if (i >= value.size() || (static_cast<unsigned char>(value[i]) & 0xc0) != 0x80) throw std::runtime_error("encoding");
            code = (code << 6) | (static_cast<unsigned char>(value[i++]) & 63); }
        if (code < minimum || code > 0x10ffff || (code >= 0xd800 && code <= 0xdfff)) throw std::runtime_error("encoding");
        if (code >= 0x10000) { code -= 0x10000; out += static_cast<char16_t>(0xd800 + (code >> 10)); out += static_cast<char16_t>(0xdc00 + (code & 1023)); }
        else out += static_cast<char16_t>(code);
    }
    return out;
}
std::string To8(const std::u16string& value) {
    std::string out;
    for (std::size_t i = 0; i < value.size(); ++i) {
        unsigned code = value[i];
        if (code >= 0xd800 && code <= 0xdbff) {
            if (++i == value.size() || value[i] < 0xdc00 || value[i] > 0xdfff) throw std::runtime_error("encoding");
            code = 0x10000 + ((code - 0xd800) << 10) + value[i] - 0xdc00;
        } else if (code >= 0xdc00 && code <= 0xdfff) throw std::runtime_error("encoding");
        if (code < 0x80) out += static_cast<char>(code);
        else if (code < 0x800) { out += static_cast<char>(0xc0 | (code >> 6)); out += static_cast<char>(0x80 | (code & 63)); }
        else if (code < 0x10000) { out += static_cast<char>(0xe0 | (code >> 12)); out += static_cast<char>(0x80 | ((code >> 6) & 63)); out += static_cast<char>(0x80 | (code & 63)); }
        else { out += static_cast<char>(0xf0 | (code >> 18)); out += static_cast<char>(0x80 | ((code >> 12) & 63)); out += static_cast<char>(0x80 | ((code >> 6) & 63)); out += static_cast<char>(0x80 | (code & 63)); }
    }
    return out;
}
}
PrivateAnswer NativeNetworkKeyboard(const PrivateText& text, const std::atomic<bool>& stop) {
    if (stop) return {EditorResult::cancelled, {}};
    Eden::TextRequest request;
    switch (text.field) {
    case Field::name: request.title = u"Network location name"; break;
    case Field::server: request.title = u"Network server IPv4"; break;
    case Field::share: request.title = u"Network share / export"; break;
    case Field::folder: request.title = u"Network games folder (relative)"; break;
    case Field::updates: request.title = u"Network updates / DLC folder (relative)"; break;
    case Field::user: request.title = u"SMB user (empty = guest)"; break;
    case Field::password: request.title = u"SMB password (private replacement)"; break;
    case Field::domain: request.title = u"SMB domain (optional)"; break;
    case Field::uid: request.title = u"NFS AUTH_SYS UID"; break;
    case Field::gid: request.title = u"NFS AUTH_SYS GID"; break;
    case Field::mount_port: request.title = u"NFS mount port (0 = portmapper)"; break;
    case Field::port: request.title = u"Network server TCP port"; break;
    case Field::cache: request.title = u"Network cache (1-32 MiB)"; break;
    case Field::timeout: request.title = u"Network timeout (250-15000 ms)"; break;
    default: return {EditorResult::invalid, {}};
    }
    request.placeholder = text.password ? u"Enter a replacement; Cancel keeps the current password" : u"Confirm to use this value; Cancel keeps the previous value";
    request.password = text.password; request.numbers = text.numbers; request.max_length = text.limit;
    try {
        request.max_length = std::min<std::size_t>(text.limit, 1023); // native IME's hard limit
        if (!text.password) request.initial = To16(text.initial);
        if (request.initial.size() > request.max_length) return {EditorResult::invalid, {}}; // never silently truncate an imported path
        const auto answer = Eden::AskSystemKeyboard(request, stop);
        if (stop || answer.outcome == Eden::TextOutcome::cancelled) return {EditorResult::cancelled, {}};
        if (answer.outcome != Eden::TextOutcome::accepted) return {};
        auto value = To8(answer.text);
        if (value.size() > text.limit) return {EditorResult::invalid, {}};
        return {EditorResult::ok, std::move(value)};
    } catch (...) { return {EditorResult::invalid, {}}; }
}
} // namespace Eden::Network
