// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "core/file_sys/vfs/vfs.h"
#include "core/loader/loader.h"
#include <array>

namespace Eden {
// Used by the derived actual ProgramMetadata::Load, including early NSP title discovery,
// not just by the later ExeFS resolver. Check before the parser sizes its capability vector.
inline Loader::ResultStatus BoundNpdmCapabilities(const FileSys::VirtualFile& npdm) {
    std::array<u8, 0x80> header{};
    if (!npdm || npdm->Read(header.data(), header.size(), 0) != header.size())
        return Loader::ResultStatus::ErrorBadNPDMHeader;
    const auto word = [](const u8* p) {
        return u32{p[0]} | u32{p[1]} << 8 | u32{p[2]} << 16 | u32{p[3]} << 24;
    };
    const u64 aci = word(header.data() + 0x70);
    std::array<u8, 0x40> access{};
    if (aci > npdm->GetSize() || access.size() > npdm->GetSize() - aci ||
        npdm->Read(access.data(), access.size(), aci) != access.size())
        return Loader::ResultStatus::ErrorBadACIHeader;
    const u64 offset = aci + word(access.data() + 0x30);
    const u64 size = word(access.data() + 0x34);
    if (size > 1024 * 1024 || size % sizeof(u32) || offset > npdm->GetSize() ||
        size > npdm->GetSize() - offset)
        return Loader::ResultStatus::ErrorBadKernelCapabilityDescriptors;
    return Loader::ResultStatus::Success;
}
} // namespace Eden
