#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Docker-only P05 storage/descriptor/codec witness; /data must be a tmpfs.

Compiles actual settings/helpers and the provider image body with system stb.
No selected core, SDK, console data or firmware is needed or qualified.
"""
from pathlib import Path
import os
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root / 'headless/prosperoeden/eden_services.cpp').read_text(encoding='utf-8')
body = source[source.index('bool EdenServices::load_image('):]
assert body.count('stbi_load_from_memory(') == 1
body = body.replace('stbi_load_from_memory(', 'WitnessDecode(')
discovery = source[source.index('bool EdenServices::independent_storage_available()'):
                   source.index('\nstd::string EdenServices::location_folder(')]
services = (root / 'headless/prosperoeden/pe/ui/services.hpp').read_text(encoding='utf-8')
fallback = services[services.index('    virtual bool content_entries('):
                    services.index('    virtual std::string location_folder(')]
code = r'''
#include <dirent.h>
#include <atomic>
#include <future>
#include <new>
static unsigned directory_calls = 0;
static unsigned streams_opened = 0, streams_closed = 0;
static int last_stream_fd = -1;
static bool throw_walk = false;
static std::atomic<bool>* cancel_walk = nullptr;
static dirent* CountReadDirectory(DIR* directory) {
    ++directory_calls;
    if (cancel_walk && directory_calls == 4) cancel_walk->store(true);
    if (throw_walk && directory_calls == 4) throw std::bad_alloc{};
    return ::readdir(directory);
}
static DIR* CountOpenDirectoryStream(int fd) {
    DIR* stream=::fdopendir(fd);
    if (stream) { ++streams_opened; last_stream_fd=fd; }
    return stream;
}
static int CountCloseDirectoryStream(DIR* stream) {
    ++streams_closed;
    return ::closedir(stream);
}
#define readdir CountReadDirectory
#define fdopendir CountOpenDirectoryStream
#define closedir CountCloseDirectoryStream
#include "assets_dir.h"
#undef readdir
#undef fdopendir
#undef closedir
#include "library_file.h"
#include "pe/gfx/image.hpp"
#include <cassert>
#include <fstream>
#include <filesystem>
#include <memory>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
static unsigned decode_entries = 0;
static stbi_uc* WitnessDecode(const stbi_uc* b, int len, int* x, int* y, int* c, int desired) {
    ++decode_entries;
    return stbi_load_from_memory(b,len,x,y,c,desired);
}
class EdenServices { public:
    bool load_image(const std::string&, pe::gfx::Image*);
    bool independent_storage_available();
    bool content_entries(const std::string&, std::vector<std::string>*, std::vector<std::string>*, const std::atomic<bool>*);
};
class DefaultContentServices { public:
FALLBACK_BODY
};
DISCOVERY_BODY
IMAGE_BODY
// Inject requests while the ACTUAL settings writer stages/closes and after its
// final decision. Real libc operations still run; this is not a PS5 I/O test.
static int cancel_stage = 0;
static std::atomic<bool>* cancel_save = nullptr;
static bool fail_metadata = false;
extern "C" int __real_rename(const char*, const char*);
extern "C" ssize_t __real_write(int, const void*, std::size_t);
extern "C" int __real_close(int);
extern "C" ssize_t __wrap_write(int fd, const void* p, std::size_t n) {
    const auto result=__real_write(fd,p,n);
    if (cancel_save && cancel_stage==1) cancel_save->store(true);
    return result;
}
extern "C" int __wrap_close(int fd) {
    const auto result=__real_close(fd);
    if (cancel_save && cancel_stage==2) cancel_save->store(true);
    return result;
}
extern "C" int __real_fstatat(int, const char*, struct stat*, int);
extern "C" int __wrap_fstatat(int fd, const char* path, struct stat* result, int flags) {
    if (fail_metadata) { errno=EIO; return -1; }
    return __real_fstatat(fd,path,result,flags);
}
extern "C" int __wrap_rename(const char* from, const char* to) {
    if (cancel_save && cancel_stage == 3) cancel_save->store(true);
    return __real_rename(from,to);
}
int main(int argc, char** argv) {
    assert(argc == 2);
    using namespace Eden;
    namespace fs = std::filesystem;
    FilesystemAccessStatus() = 0;
    if (std::string(argv[1]) == "reopen") {
        assert(GamesDir() == "/data/base/roms");
        assert(WallpapersDir() == "/data/base/wallpapers");
        assert(WallpaperName().empty());
        std::puts("CONTENT_ROOT_REOPEN_PASS");
        return 0;
    }
    if (std::string(argv[1]) == "selected") {
        assert(GamesDir() == "/data/fixture/games");
        assert(WallpapersDir() == "/data/fixture/pictures");
        assert(WallpaperName() == "sample.tga");
        assert(SaveContentFolder(false,""));
        assert(LoadWallpaper() == "sample.tga"); // reset games does not reset wallpaper
        assert(GamesDir() == "/data/fixture/games"); // currently active root stays frozen
        assert(SaveContentFolder(true,""));
        assert(LoadSavedAssetsDir() == "/data/base");
        assert(Settings::Load(SettingsFile()).at("future") == 17);
        std::puts("CONTENT_INDEPENDENT_SELECTED_REOPEN_AND_RESET_PASS");
        return 0;
    }
    fs::create_directories(ConfigDir());
    const std::string folder = "/data/fixture";
    fs::create_directories(folder + "/games");
    fs::create_directories(folder + "/pictures");
    auto original = Settings::Json{{"game_files","/data/base"},{"future",17},
        {"library",{{"last_game","Title/main"}}}};
    assert(Settings::Write(original, SettingsFile()));
    assert(SaveContentFolder(false, folder + "/games"));
    assert(LoadContentFolder(false) == folder + "/games");
    assert(LoadSavedAssetsDir() == "/data/base");
    assert(GamesDir() == folder + "/games");
    assert(GamePath("Title/main") == folder + "/games/Title/main");
    assert(!SaveContentFolder(false, "/etc"));
    assert(!SaveContentFolder(false, "/mnt/a/../b"));
    assert(!SaveContentFolder(false, "relative"));
    assert(SaveWallpaper(folder + "/pictures", "sample.tga"));
    assert(WallpaperName().empty() && WallpapersDir() == "/data/base/wallpapers"); // same early snapshot as GamesDir
    assert(!SaveWallpaper(folder, "../sample.tga"));
    assert(LoadWallpaper() == "sample.tga");
    assert(Settings::Load(SettingsFile()).at("future") == 17);
    assert(Settings::Load(SettingsFile()).at("library").at("last_game") == "Title/main");
    assert(UserContent::ImageName("photo.JPEG"));
    assert(!UserContent::ImageName("bad.png/other"));
    assert(!UserContent::ImageName("../bad.png"));
    std::vector<unsigned char> tga(30, 127);
    std::fill(tga.begin(),tga.begin()+18,0);
    tga[2]=2; tga[12]=2; tga[14]=2; tga[16]=24; tga[17]=0x20;
    const auto write = [&](const std::string& name, const auto& bytes) {
        std::ofstream out(folder+"/pictures/"+name,std::ios::binary);
        out.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    };
    write("sample.tga",tga);
    std::vector<unsigned char> bytes;
    std::atomic<bool> cancel{true};
    assert(UserContent::OpenDirectory(folder,&cancel) == -1);
    assert(UserContent::Images(folder,&cancel).empty());
    assert(!UserContent::ReadImage(folder+"/pictures","sample.tga",bytes,&cancel));
    assert(UserContent::ReadImage(folder+"/pictures","sample.tga",bytes) && bytes==tga);
    fs::create_symlink(folder+"/pictures/sample.tga",folder+"/pictures/link.tga");
    fs::create_directory_symlink(folder+"/pictures",folder+"/alias");
    fs::create_hard_link(folder+"/pictures/sample.tga",folder+"/pictures/hard.tga");
    assert(!UserContent::ReadImage(folder+"/pictures","sample.tga",bytes)); // hardlink aliases are refused
    fs::remove(folder+"/pictures/hard.tga");
    assert(!UserContent::ReadImage(folder+"/pictures","link.tga",bytes));
    assert(!UserContent::ReadImage(folder+"/alias","sample.tga",bytes));
    assert(::mkfifo((folder+"/pictures/pipe.png").c_str(),0600)==0);
    assert(!UserContent::ReadImage(folder+"/pictures","pipe.png",bytes));
    write("large.tga",tga);
    fs::resize_file(folder+"/pictures/large.tga",UserContent::kMaxImageBytes+1);
    assert(!UserContent::ReadImage(folder+"/pictures","large.tga",bytes));
    const auto images=UserContent::Images(folder+"/pictures");
    assert(images.size()==1 && images[0]=="sample.tga");
    EdenServices service; pe::gfx::Image image;
    assert(service.load_image("wallpaper:"+folder+"/pictures/sample.tga",&image));
    assert(image.width==2 && image.height==2 && image.rgba.size()==16);
    const auto successful_decode_entries=decode_entries;
    assert(successful_decode_entries==1);
    tga[12]=1; tga[13]=16; // 4097px, refused by info BEFORE full decode/allocation
    write("oversize.tga",tga);
    assert(!service.load_image("wallpaper:"+folder+"/pictures/oversize.tga",&image));
    assert(decode_entries==successful_decode_entries);
    tga[12]=0xb8; tga[13]=0x0b; tga[14]=0xb8; tga[15]=0x0b; // 3000x3000: per-side valid, total too large
    write("pixels.tga",tga);
    assert(!service.load_image("wallpaper:"+folder+"/pictures/pixels.tga",&image));
    assert(decode_entries==successful_decode_entries); // reject dimensions before entering full decoder
    assert(!service.load_image("wallpaper:"+folder+"/pictures/link.tga",&image));
    auto exported=Settings::Json::parse(LibraryFile::Text("/data/base",{},GamesDir()));
    assert(exported.at("game_files")=="/data/base" && exported.at("games_folder")==GamesDir());
    for (int i=0;i<140;++i) write("generated-"+std::to_string(i)+".tga",tga);
    const auto capped=UserContent::Images(folder+"/pictures");
    assert(capped.size()==128 && std::is_sorted(capped.begin(),capped.end()));
    std::vector<std::string> directories, offered;
    DefaultContentServices unavailable;
    directories={"stale folder"}; offered={"stale image"};
    assert(!unavailable.content_entries(folder, &directories, &offered, nullptr));
    assert(directories.empty() && offered.empty());
    FilesystemAccessStatus()=1;
    directories={"stale folder"}; offered={"stale image"};
    assert(!service.content_entries(folder, &directories, &offered, nullptr));
    assert(directories.empty() && offered.empty());
    FilesystemAccessStatus()=0;
    assert(service.content_entries(folder+"/pictures", &directories, &offered, nullptr));
    assert(directories.empty() && offered.size()==128);
    fs::create_directory(folder+"/entry-limit");
    for(int i=0;i<4100;++i) fs::create_directory(folder+"/entry-limit/n"+std::to_string(i));
    directory_calls=0;
    assert(UserContent::Images(folder+"/entry-limit").empty() && directory_calls==4096);
    directory_calls=0;
    assert(service.content_entries(folder+"/entry-limit", &directories, &offered, nullptr));
    assert(directory_calls==4096 && directories.size()==256 && offered.empty());
    directory_calls=0; cancel_walk=&cancel; cancel.store(false);
    assert(!service.content_entries(folder+"/entry-limit", &directories, &offered, &cancel));
    assert(directory_calls==4 && directories.empty() && offered.empty());
    cancel_walk=nullptr; cancel.store(false);
    fail_metadata=true;
    assert(!service.content_entries(folder+"/entry-limit", &directories, &offered, nullptr));
    assert(directories.empty() && offered.empty());
    fail_metadata=false;
    for(int fault=0;fault<8;++fault) {
        directories={"stale folder"}; offered={"stale image"};
        const auto opened=streams_opened, closed=streams_closed;
        directory_calls=0; throw_walk=true;
        auto discovery=std::async(std::launch::async,[&] {
            return service.content_entries(folder+"/entry-limit", &directories, &offered, nullptr);
        });
        assert(!discovery.get()); // exception after stream acquisition becomes whole-walk failure
        throw_walk=false;
        assert(directory_calls==4 && streams_opened==opened+1 && streams_closed==closed+1);
        assert(directories.empty() && offered.empty());
        errno=0;
        assert(::fcntl(last_stream_fd,F_GETFD)==-1 && errno==EBADF); // actual duplicated descriptor was closed
    }
    std::puts("CONTENT_DISCOVERY_FUTURE_EXCEPTION_CLOSES_STREAM_AND_CLEARS_OUTPUTS_PASS");
    assert(service.content_entries("/", &directories, nullptr, nullptr)); // navigation ancestry, not save admission
    const auto malformed=SettingsFile()+".malformed";
    assert(Settings::Write({{"storage",17}},malformed));
    assert(!SaveContentFolder(false,folder,malformed));
    assert(!SaveWallpaper(folder,"sample.tga",malformed));
    assert(Settings::Load(malformed).at("storage")==17);
    for (const std::string text : {"[]", "17", "null", "{\"storage\":"}) {
        assert(Settings::WriteFile(malformed,text));
        assert(!SaveContentFolder(false,folder,malformed));
        assert(!SaveWallpaper(folder,"sample.tga",malformed));
        std::string unchanged; assert(Settings::ReadFile(malformed,unchanged) && unchanged==text);
    }
    const auto publication=SettingsFile()+".publication";
    assert(Settings::Write({{"preserved",31}},publication));
    std::string old; assert(Settings::ReadFile(publication,old));
    fs::create_symlink(publication,publication+".tmp");
    cancel_save=&cancel;
    for (int at : {1,2}) {
        cancel.store(false); cancel_stage=at;
        assert(!SaveContentFolder(false,folder,publication,&cancel));
        std::string unchanged; assert(Settings::ReadFile(publication,unchanged) && unchanged==old);
        assert(fs::is_symlink(publication+".tmp")); // never follow/remove an unowned legacy staging alias
        cancel.store(false);
        assert(!SaveWallpaper(folder,"sample.tga",publication,&cancel));
        assert(Settings::ReadFile(publication,unchanged) && unchanged==old);
    }
    cancel.store(false); cancel_stage=3;
    assert(SaveWallpaper(folder,"sample.tga",publication,&cancel)); // cancellation AFTER cutoff cannot undo rename
    assert(cancel.load() && LoadWallpaper(publication)=="sample.tga");
    assert(Settings::Load(publication).at("preserved")==31);
    cancel_save=nullptr;
    fs::remove(publication+".tmp");
    fs::create_hard_link(publication,publication+".tmp");
    assert(SaveContentFolder(false,folder,publication));
    std::string retained; assert(Settings::ReadFile(publication+".tmp",retained));
    assert(Settings::Json::parse(retained).at("storage").at("wallpaper")=="sample.tga");
    assert(Settings::Load(publication).at("storage").at("games_folder")==folder);
    const auto blocked=SettingsFile()+".collision";
    assert(Settings::Write({{"do_not_touch",67}},blocked));
    std::vector<std::string> aliases;
    for(unsigned i=0;i<512;++i) {
        const auto alias=blocked+".tmp."+std::to_string(::getpid())+"."+std::to_string(i);
        if(i%2) fs::create_symlink(blocked,alias); else fs::create_hard_link(blocked,alias);
        aliases.push_back(alias);
    }
    std::string protected_before; assert(Settings::ReadFile(blocked,protected_before));
    assert(!SaveContentFolder(false,folder,blocked)); // all eight exclusive candidates already belong to someone else
    std::string protected_after; assert(Settings::ReadFile(blocked,protected_after) && protected_after==protected_before);
    for(const auto& alias:aliases) {
        assert(fs::exists(alias)); // writer must not remove a node it did not create
        fs::remove(alias);
    }
    std::puts("CONTENT_PROVIDER_ENUMERATION_AND_PUBLICATION_BOUNDARY_PASS");
    std::puts("CONTENT_ROOT_DESCRIPTOR_CODEC_PASS");
}
'''.replace('IMAGE_BODY', body).replace('DISCOVERY_BODY', discovery).replace('FALLBACK_BODY', fallback)
native_gate = r'''
#define PS5_NATIVE 1
#include "user_content.h"
#include <cassert>
int main() {
    static_assert(!Eden::UserContent::kIoAvailable);
    assert(Eden::UserContent::OpenDirectory("/data/test")==-1);
    std::vector<unsigned char> bytes{1};
    assert(!Eden::UserContent::ReadImage("/data/test","x.png",bytes) && bytes.empty());
    assert(Eden::UserContent::Images("/data/test").empty());
    std::vector<std::string> folders;
    assert(!Eden::UserContent::Entries("/data/test",&folders,nullptr));
}
'''
with tempfile.TemporaryDirectory(prefix='encore-content-') as tmp:
    for name, unit, extra in [('content', code, [str(root/'headless/prosperoeden/pe/gfx/image.cpp')]),
                              ('native-gate', native_gate, []),
                              ('native-gate-zero', '#define EDEN_NATIVE_USER_CONTENT_IO_QUALIFIED 0\n'+native_gate, [])]:
        exe = Path(tmp) / name
        subprocess.run([os.environ.get('CXX','c++'), '-std=c++20', '-Wall', '-Wextra', '-pthread',
            '-fsanitize=address,undefined', '-fno-sanitize-recover=all',
            '-I'+str(root/'headless'), '-I'+str(root/'headless/prosperoeden'), '-I/usr/include/stb',
            '-x','c++','-', *extra,
            *(['-Wl,--wrap=write,--wrap=close,--wrap=rename,--wrap=fstatat'] if name=='content' else []),
            '-o',str(exe)], input=unit,text=True,check=True)
        subprocess.run([str(exe),'test'],check=True,timeout=30)
        if name=='content':
            subprocess.run([str(exe),'selected'],check=True,timeout=30)
            subprocess.run([str(exe),'reopen'],check=True,timeout=30)
        else:
            imports=subprocess.check_output(['nm','-u',str(exe)],text=True)
            assert all(symbol not in imports for symbol in ['openat','fdopendir','readdir','fstatat'])
            print('UNQUALIFIED_NATIVE_CONTENT_IO_HAS_NO_NEW_DESCRIPTOR_IMPORTS_PASS')
