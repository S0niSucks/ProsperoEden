# SPDX-License-Identifier: GPL-3.0-or-later
# Composes with the existing multiplayer/native socketpair derivative, never overwrites it.
function(nextendo_native_network variable)
    set(source "${${variable}}")
    string(PREPEND source "#include \"nextendo.h\"\n#include \"nextendo_native_network.h\"\n")
    nextendo_replace(source "s32 gai_err = getaddrinfo(host.c_str(), service.has_value() ? service->c_str() : nullptr, &hints, &addrinfo);"
        "Eden::Nextendo::NativeLookup lookup(Eden::Nextendo::GuestEnabled());\n    s32 gai_err = lookup.Resolve(host.c_str(), service.has_value() ? service->c_str() : nullptr, &hints, &addrinfo);" 1)
    nextendo_replace(source "    freeaddrinfo(addrinfo);" "    lookup.Free(addrinfo);" 1)
    set(${variable} "${source}" PARENT_SCOPE)
endfunction()
