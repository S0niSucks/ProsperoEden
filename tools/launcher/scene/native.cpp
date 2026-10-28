// SPDX-License-Identifier: GPL-3.0-or-later
// Host driver for the same bounded C++ backend compiled into the console launcher.
#include "pe/ui/scene/nx_install.hpp"
#include "pe/ui/scene/player.hpp"
#include <filesystem>
#include <iostream>
using namespace pe::ui::scene;
using pe::ui::ThemeImportStorage;
int main(int argc,char** argv) {
    try {
        require(argc>=3,"usage: native inspect|stage|install SOURCE DATA [consent [partial]]; recover DATA; layout FILE PATCH");
        const std::string mode=argv[1];if(mode=="recover"||mode=="recover-denied"){ThemeImportStorage deny=[](const std::string&,std::uint64_t)->std::shared_ptr<void>{return {};};std::cout<<nx::recover(argv[2],mode=="recover-denied"?deny:ThemeImportStorage{}).dump()<<'\n';return 0;}
        if(mode=="animation"){const std::filesystem::path path=argv[2];Reader reader(path.parent_path().string());std::cout<<nx::bflan(reader.read(path.filename().string(),nx::member_limit)).dump()<<'\n';return 0;}
        if(mode=="project") {
            require(argc>=4,"project needs screen");auto package=load(argv[2]);package.model.entry=argv[3];Player player(std::move(package.model));
            const float seconds=argc>=5?std::stof(argv[4]):0;const bool calm=argc>=6&&std::string_view(argv[5])=="calm";
            auto query=[](const std::string&)->Json{return Json::object();};auto sound=[](const std::string&,bool){};
            player.update(0,calm);
            for(float elapsed=0;elapsed<seconds;){const auto dt=std::min(0.025f,seconds-elapsed);player.update(dt,calm);elapsed+=dt;}
            Json commands=Json::array();player.draw(query,[&](const std::string& type,const Json& props){if(type=="rect"||type=="border")commands.push_back(props);},sound);
            std::cout<<commands.dump()<<'\n';return 0;
        }
        const std::filesystem::path path=argv[2];Reader reader(path.parent_path().string());
        if(mode=="texture"||mode=="fuzz-texture") {
            const auto bytes=reader.read(path.filename().string(),nx::archive_limit);
            if(mode=="texture"){const auto image=nx::bntx_texture(bytes,argc>=4?argv[3]:"Texture");std::cout<<Json{{"width",image.width},{"height",image.height},{"rgba_sha256",nx::sha256(std::string_view(reinterpret_cast<const char*>(image.rgba.data()),image.rgba.size()))}}.dump()<<'\n';}
            else {unsigned rejected=0;for(unsigned i=0;i<2000;++i){auto damaged=bytes;const auto at=std::size_t(i*167u+13)%damaged.size();damaged[at]^=char(1u<<(i%8));if(i%3==0)damaged.resize(at);try{(void)nx::bntx_texture(damaged,"Texture");}catch(const std::exception&){++rejected;}}std::cout<<Json{{"mutations",2000},{"rejected",rejected}}.dump()<<'\n';}return 0;
        }
        if(mode=="fuzz") {
            const auto source=reader.read(path.filename().string(),nx::archive_limit);std::uint32_t seed=239173;unsigned accepted=0,rejected=0;
            for(unsigned iteration=0;iteration<2000;++iteration){auto bytes=source;seed=seed*1664525+1013904223;
                const auto at=seed%std::min<std::size_t>(bytes.size(),iteration%2?bytes.size():512);bytes[at]^=char(1u<<(seed%8));
                if(iteration%7==0)bytes.resize(seed%bytes.size());
                try{(void)nx::convert(bytes);++accepted;}catch(const std::exception&){++rejected;}
            }
            std::cout<<Json{{"mutations",2000},{"accepted",accepted},{"rejected",rejected}}.dump()<<'\n';return 0;
        }
        if(mode=="layout") {
            require(argc>=4,"layout patch argument missing");const std::filesystem::path p=argv[3];Reader patches(p.parent_path().string());Json omissions=Json::array();
            auto layout=nx::bflyt(reader.read(path.filename().string(),nx::member_limit),path.filename().string(),omissions);
            nx::patch(layout,patches.json(p.filename().string()),path.filename().string(),omissions);const auto geometry=nx::geometry(layout,path.filename().string(),omissions);Json out=Json::array();
            for(std::size_t i=0;i<geometry.size();++i){const auto& g=geometry[i];out.push_back({{"name",layout.panes[i].name},{"cx",g.cx},{"cy",g.cy},{"sx",g.sx},{"sy",g.sy},{"alpha",g.alpha},{"visible",g.visible}});}
            std::cout<<Json{{"panes",out},{"omissions",omissions}}.dump()<<'\n';return 0;
        }
        require(argc>=4,"data root argument missing");const std::string root=argv[3];
        auto plan=nx::convert(reader.read(path.filename().string(),nx::archive_limit),[&](const std::string& target){Reader firmware(root+"/theme-import/firmware");return firmware.read(target+".szs",nx::archive_limit);});
        auto report=plan.report;report["id"]=plan.id;
        if(mode=="storage") {
            unsigned calls=0,live=0;struct Credit{unsigned& live;explicit Credit(unsigned& value):live(value){++live;}~Credit(){--live;}};
            ThemeImportStorage deny=[&](const std::string& where,std::uint64_t bytes)->std::shared_ptr<void>{++calls;require(where==root&&bytes==nx::storage_bytes(plan.files),"incorrect reservation arguments");return {};};
            bool refused=false;try{(void)nx::install(root,plan,true,true,deny);}catch(const std::exception&){refused=true;}
            require(refused&&calls==1&&!std::filesystem::exists(root+"/theme-import/staged")&&!std::filesystem::exists(root+"/launchers/"+plan.id),"denied reservation wrote files");
            ThemeImportStorage accept=[&](const std::string&,std::uint64_t)->std::shared_ptr<void>{++calls;require(live==0,"nested storage reservation");return std::make_shared<Credit>(live);};
            (void)nx::install(root,plan,true,true,accept);require(calls==2&&live==0,"reservation was not released exactly once");
            auto invalid=plan;invalid.id+="-invalid";auto manifest=nx::json(invalid.files.at("manifest.json"));manifest["id"]=invalid.id;invalid.files["manifest.json"]=manifest.dump();invalid.files["theme.json"]="{}";
            bool failed=false;try{(void)nx::install(root,invalid,true,true,accept);}catch(const std::exception&){failed=true;}
            require(failed&&calls==3&&live==0,"failed staging leaked reservation credit");
            {auto local=nx::reserve_storage(root,1,{});bool rejected=false;try{(void)nx::reserve_storage(root,UINT64_MAX,{});}catch(const std::exception&){rejected=true;}require(rejected,"local overcapacity accepted");}
            report["reservation_calls"]=calls;report["released"]=true;report["reserved_bytes"]=nx::storage_bytes(plan.files);
        }
        else if(mode=="install") report["installed"]=nx::install(root,plan,argc>=5&&std::string_view(argv[4])=="consent",argc>=6&&std::string_view(argv[5])=="partial");
        else if(mode=="stage")report["staged"]=nx::stage(root,plan);
        else require(mode=="inspect","unknown mode");
        std::cout<<report.dump()<<'\n';return 0;
    } catch(const std::exception& e){std::cout<<Json{{"error",e.what()}}.dump()<<'\n';return 1;}
}
