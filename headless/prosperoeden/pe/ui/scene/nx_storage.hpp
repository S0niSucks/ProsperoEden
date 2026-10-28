// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "pe/ui/scene/nx_archive.hpp"
#include "pe/ui/theme_import_storage.hpp"
#include <mutex>
#include <sys/statvfs.h>
namespace pe::ui::scene::nx {
inline std::uint64_t storage_bytes(const Files& files) {
    require(files.size()<=128,"import storage file-count budget");std::uint64_t total=0,largest=0;
    for(const auto& [name,bytes]:files){(void)name;total+=bytes.size();largest=std::max(largest,std::uint64_t(bytes.size()));require(total<=128*1024*1024,"import storage byte budget");}
    return 2*total+largest+1024*1024; // retained stage + installed copy + largest temporary + metadata
}
struct LocalStorageCredit {
    std::string root;std::uint64_t bytes{};
    struct Ledger {std::mutex mutex;std::map<std::string,std::uint64_t> held;};
    static Ledger& ledger(){static Ledger value;return value;}
    ~LocalStorageCredit(){if(!bytes)return;auto& value=ledger();std::lock_guard lock(value.mutex);const auto it=value.held.find(root);if(it!=value.held.end()){it->second-=bytes;if(!it->second)value.held.erase(it);}}
};
inline std::shared_ptr<void> reserve_storage(const std::string& root,std::uint64_t bytes,const ThemeImportStorage& external) {
    if(external){auto token=external(root,bytes);require(bool(token),"theme import storage reservation denied");return token;}
    auto token=std::make_shared<LocalStorageCredit>();token->root=root;
    auto& ledger=LocalStorageCredit::ledger();std::lock_guard lock(ledger.mutex);struct statvfs status{};
    require(statvfs(root.c_str(),&status)==0,"cannot inspect theme import storage");const auto block=status.f_frsize?status.f_frsize:status.f_bsize;
    require(block>0&&status.f_bavail<=UINT64_MAX/block,"invalid theme import storage capacity");const auto available=std::uint64_t(status.f_bavail)*block;
    const auto found=ledger.held.find(root);const auto held=found==ledger.held.end()?0:found->second;
    require(held<=available&&bytes<=available-held,"not enough reserved storage for theme import");
    ledger.held[root]=held+bytes;token->bytes=bytes;return token;
}
} // namespace pe::ui::scene::nx
