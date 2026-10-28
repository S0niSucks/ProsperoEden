// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "pe/ui/scene/nx_import.hpp"
#include <functional>

namespace pe::ui::scene::nx {
struct Fd {
    int value=-1;explicit Fd(int fd):value(fd){require(value>=0,"cannot open import directory/file");}~Fd(){close(value);}
    Fd(const Fd&)=delete;Fd& operator=(const Fd&)=delete;
};
inline void sync_dir(int fd) {require(fsync(fd)==0||errno==EINVAL||errno==ENOTSUP,"cannot synchronize import directory");}
inline int child_dir(int parent,const std::string& name,bool create=true) {
    require(runtime_fs::relative_path(name)&&name.find('/')==name.npos,"invalid import directory");
    if(create)require(mkdirat(parent,name.c_str(),0755)==0||errno==EEXIST,"cannot create import directory");
    return openat(parent,name.c_str(),O_RDONLY|O_DIRECTORY|O_NOFOLLOW);
}
inline bool resource_name(const std::string& name) {
    if(!runtime_fs::relative_path(name))return false;
    const auto slash=name.find('/');if(slash!=name.npos && (name.find('/',slash+1)!=name.npos || (name.substr(0,slash)!="assets"&&name.substr(0,slash)!="screens"&&name.substr(0,slash)!="animations")))return false;
    return name.ends_with(".json")||name.ends_with(".tga");
}
inline void publish(int root,const std::string& path,std::string_view bytes) {
    require(resource_name(path),"invalid converted resource path");Fd parent(dup(root));const auto slash=path.find('/');int fd=parent.value;std::unique_ptr<Fd> child;
    if(slash!=path.npos){child=std::make_unique<Fd>(child_dir(fd,path.substr(0,slash)));fd=child->value;}
    const auto name=slash==path.npos?path:path.substr(slash+1);std::string tmp;int raw=-1;
    for(int i=0;i<16&&raw<0;++i){tmp=".nx-tmp-"+std::to_string(i)+"-"+name;raw=openat(fd,tmp.c_str(),O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW,0600);if(raw<0&&errno!=EEXIST)break;}
    require(raw>=0,"cannot create exclusive import resource");Fd output(raw);
    try {
        for(std::size_t at=0;at<bytes.size();) {const auto n=write(raw,bytes.data()+at,bytes.size()-at);if(n<0&&errno==EINTR)continue;require(n>0,"cannot write import resource");at+=n;}
        require(fsync(raw)==0,"cannot synchronize import resource");
        // linkat is an atomic no-replace publication on both host POSIX and supported console
        // mounts. Never downgrade to an overwrite-capable rename on a mount lacking hardlinks.
        require(linkat(fd,tmp.c_str(),fd,name.c_str(),0)==0,"cannot publish import resource (exists, or mount lacks atomic no-clobber links)");
        require(unlinkat(fd,tmp.c_str(),0)==0,"cannot finish import resource");sync_dir(fd);
    } catch(...) {unlinkat(fd,tmp.c_str(),0);throw;}
}
inline void verify_manifest(const Json& m,const std::string& id) {
    require(identifier(id)&&id!="builtin"&&m.at("id")==id&&m.at("schema")==1&&m.at("scene")=="theme.json"&&m.at("host_contract")=="launcher-v1.0"&&m.at("api").at("major")==1&&m.at("api").at("min_minor")==1,"invalid imported manifest");
    require(!m.contains("entry")&&!m.contains("font")&&!m.contains("loading_scene")&&!m.contains("sounds")&&!m.contains("assets"),"imported pack must remain script-free");
    require(m.at("required_capabilities")==Json::array({"themes.scene.v1","actions.launch.v1"}),"invalid import capabilities");
}
inline std::string stage(const std::string& root,const Conversion& plan) {
    runtime_fs::Descriptor data(root,runtime_fs::Kind::directory);Fd imports(child_dir(data.get(),"theme-import"));Fd staged(child_dir(imports.value,"staged"));
    require(runtime_fs::entries(root+"/theme-import/staged").size()<64,"too many staged imports; inspect incomplete imports before continuing");
    std::string pattern=root+"/theme-import/staged/nx-XXXXXX";require(mkdtemp(pattern.data())!=nullptr,"cannot reserve import staging directory");
    runtime_fs::Descriptor directory(pattern,runtime_fs::Kind::directory);for(const auto& [name,bytes]:plan.files)publish(directory.get(),name,bytes);
    auto package=load(pattern);require(package.model.id==plan.id,"staged scene ID mismatch");verify_manifest(json(plan.files.at("manifest.json")),plan.id);sync_dir(directory.get());sync_dir(staged.value);return pattern;
}
inline void complete(const std::string& root,const std::string& id,const Json& journal) {
    require(journal.at("schema")==1&&journal.at("id")==id,"invalid import recovery journal");const auto name=text(journal,"staged","",32);
    require(name.starts_with("nx-")&&name.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-")==name.npos,"invalid import recovery source");
    const auto staged=root+"/theme-import/staged/"+name,target=root+"/launchers/"+id;Reader source(staged),destination(target);runtime_fs::Descriptor dir(target,runtime_fs::Kind::directory);
    const auto manifest=json(source.read("manifest.json",512*1024));verify_manifest(manifest,id);
    const auto& hashes=journal.at("files");require(hashes.is_object()&&hashes.size()<=128&&hashes.contains("manifest.json"),"invalid import recovery file table");
    for(auto it=hashes.begin();it!=hashes.end();++it) {
        require(resource_name(it.key())&&it.value().is_string(),"unsafe import recovery resource");if(it.key()=="manifest.json")continue;
        const auto bytes=source.read(it.key(),archive_limit);require(sha256(bytes)==it.value().get<std::string>(),"staged import changed: "+it.key());
        struct stat exists{};const bool present=fstatat(dir.get(),it.key().c_str(),&exists,AT_SYMLINK_NOFOLLOW)==0;
        if(present)require(sha256(destination.read(it.key(),archive_limit))==it.value().get<std::string>(),"existing import resource differs; refusing to overwrite: "+it.key());
        else {require(errno==ENOENT,"cannot inspect import destination");publish(dir.get(),it.key(),bytes);}
    }
    auto package=load(target);require(package.model.id==id,"recovered scene ID mismatch");const auto bytes=source.read("manifest.json",512*1024);
    require(sha256(bytes)==hashes.at("manifest.json").get<std::string>(),"staged manifest changed");struct stat published{};
    if(fstatat(dir.get(),"manifest.json",&published,AT_SYMLINK_NOFOLLOW)==0)require(sha256(destination.read("manifest.json",512*1024))==hashes.at("manifest.json").get<std::string>(),"live manifest differs; refusing to overwrite");
    else {require(errno==ENOENT,"cannot inspect imported manifest");publish(dir.get(),"manifest.json",bytes);}
    require(unlinkat(dir.get(),"import-pending.json",0)==0,"cannot finish import journal");sync_dir(dir.get());
}
inline Json recover(const std::string& root) {
    Json result=Json::array();std::vector<std::string> installed;try{installed=runtime_fs::entries(root+"/launchers");}catch(...){return result;}
    for(const auto& id:installed)if(identifier(id)&&id!="builtin") {
        try {runtime_fs::Descriptor dir(root+"/launchers/"+id,runtime_fs::Kind::directory);struct stat s{};
            if(fstatat(dir.get(),"import-pending.json",&s,AT_SYMLINK_NOFOLLOW)!=0)continue;
            Reader reader(root+"/launchers/"+id);const auto journal=reader.json("import-pending.json");complete(root,id,journal);result.push_back({{"id",id},{"recovered",true}});
        }catch(const std::exception& e){result.push_back({{"id",id},{"recovered",false},{"error",e.what()}});}
    }return result;
}
inline std::string install(const std::string& root,const Conversion& plan,bool consent,bool partial_consent=false) {
    require(consent,"explicit native import consent is required");require(plan.report.at("omissions").empty()||partial_consent,"explicit consent to listed omissions is required");
    verify_manifest(json(plan.files.at("manifest.json")),plan.id);runtime_fs::Descriptor data(root,runtime_fs::Kind::directory);Fd launchers(child_dir(data.get(),"launchers"));
    struct stat existing{};require(fstatat(launchers.value,plan.id.c_str(),&existing,AT_SYMLINK_NOFOLLOW)!=0&&errno==ENOENT,"launcher ID is already installed; no files changed");
    const auto staged=stage(root,plan);Json hashes=Json::object();for(const auto& [name,bytes]:plan.files)hashes[name]=sha256(bytes);
    const Json journal={{"schema",1},{"id",plan.id},{"staged",staged.substr(staged.find_last_of('/')+1)},{"files",hashes}};
    require(mkdirat(launchers.value,plan.id.c_str(),0755)==0,"launcher ID appeared during staging; refusing to replace it");sync_dir(launchers.value);
    Fd target(child_dir(launchers.value,plan.id,false));publish(target.value,"import-pending.json",journal.dump());
    complete(root,plan.id,journal);sync_dir(launchers.value);return plan.id;
}
} // namespace pe::ui::scene::nx
