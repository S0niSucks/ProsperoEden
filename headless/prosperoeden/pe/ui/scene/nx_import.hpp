// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "pe/ui/scene/nx_image.hpp"
#include "pe/ui/scene/nx_layout.hpp"
#include "pe/ui/scene/nx_animation.hpp"

namespace pe::ui::scene::nx {
inline const std::map<std::string,std::string> targets{{"home","Library"},{"lock","Welcome"},{"apps","All games"},{"set","Settings"},{"news","News"},{"user","Profiles"},{"psl","Choose player"}};
struct Conversion {
    std::string id,name;Files files;Json report={{"schema",1},{"faithful_parity",false},{"omissions",Json::array()},{"parts",Json::array()}};
    gfx::Image preview;
};
inline Json label(const std::string& id,const std::string& text,float x,float y,float width,float size=28) {
    return {{"id",id},{"type","text"},{"text",text},{"x",x},{"y",y},{"w",width},{"size",size}};
}
inline Json screen(const std::string& target,const std::string& image_name) {
    Json nodes=Json::array();nodes.push_back({{"id","wallpaper"},{"type","image"},{"asset",image_name},{"w",1920},{"h",1080}});
    nodes.push_back({{"id","scrim"},{"type","rect"},{"w",1920},{"h",1080},{"color",0},{"alpha",0.72}});
    nodes.push_back(label("title",targets.at(target),140,140,1600,48));
    nodes.push_back(label("notice","Imported artwork · Original ProsperoEden workflows",140,215,1600));
    nodes.push_back(label("hints","L1 / R1: parts · Back: library · Options: choose launcher",140,1020,1600));
    std::string focus;
    if(target=="home"||target=="apps") {
        focus="games";nodes.push_back({{"id","games"},{"type","grid"},{"x",140},{"y",300},{"w",1650},{"h",600},{"columns",6},{"rows",1},{"cell_w",250},{"cell_h",310},{"gap",24},{"source","/library/games"},
            {"activate",{{"action","launch.request"},{"args",{{"game",{{"$bind","/selected/handle"}}}}}}},
            {"children",Json::array({{{"id","cover"},{"type","cover"},{"w",250},{"h",250},{"bind","/item/handle"}},{{"id","name"},{"type","text"},{"y",295},{"w",245},{"bind","/item/name"}}})}});
    } else if(target=="user"||target=="psl") {
        focus="profiles";auto name=label("name","",24,58,1400,32);name["bind"]="/item/name";
        nodes.push_back({{"id","profiles"},{"type","list"},{"x",140},{"y",300},{"w",1600},{"h",600},{"rows",5},{"cell_w",1500},{"cell_h",90},{"source","/profiles"},
            {"activate",{{"action","profile.choose"},{"args",{{"handle",{{"$bind","/selected/handle"}}}}}}},{"children",Json::array({name})}});
    } else if(target=="set") {
        focus="large";int i=0;
        for(auto [field,title]:std::initializer_list<std::pair<const char*,const char*>>{{"large_text","Larger text"},{"high_contrast","High contrast"},{"reduce_motion","Reduce motion"},{"menu_volume","Menu volume (+10, wraps to Off)"}}) {
            const auto id=std::string(field)=="large_text"?"large":std::string(field)=="high_contrast"?"contrast":std::string(field)=="reduce_motion"?"calm":"volume";
            auto value=label(std::string(id)+"-value","",1400,56,300,30);value["align"]="right";value["bind"]="/preferences/"+std::string(field);value["labels"]={{"true","On"},{"false","Off"}};
            nodes.push_back({{"id",id},{"type","focus"},{"x",140},{"y",300+i++*130},{"w",1600},{"h",90},{"activate",{{"action","preferences.adjust"},{"args",{{"field",field}}}}},{"children",Json::array({label(std::string(id)+"-label",title,24,56,1100,30),value})}});
        }
    } else nodes.push_back(label("unmapped","No corresponding console service is impersonated. Back returns to the library.",140,400,1600,30));
    return {{"focus",focus},{"nodes",nodes},{"input",{{"back",{{"screen","home"}}}}}};
}
using Firmware=std::function<std::string(const std::string&)>;
inline Conversion convert(std::string_view source,const Firmware& firmware={}) {
    Conversion out;const auto digest=sha256(source);out.id="nx-"+digest.substr(0,20);out.report["source_sha256"]=digest;
    auto files=container(source);std::vector<Files> parts;
    if(files.contains("info.json"))parts.push_back(std::move(files));
    else {require(files.size()>0&&files.size()<=7,"pack must contain at most seven NXTheme parts");for(const auto& [path,bytes]:files){require(folded(path).ends_with(".nxtheme"),"pack member is not .nxtheme: "+path);parts.push_back(container(bytes));}}
    Json images=Json::object(),screens=Json::object(),models=Json::object(),reconstruction=Json::array(),animations=Json::object(),animation_files=Json::object();std::set<std::string> seen;
    for(const auto& part:parts) {
        require(part.contains("info.json"),"NXTheme info.json missing");const auto info=json(part.at("info.json"));
        require(info.is_object() && info.contains("Version") && info.at("Version").is_number_integer() && info.at("Version").get<int>()>=1 && info.at("Version").get<int>()<=17,"unsupported NXTheme Version; supported 1–17");
        const auto target=text(info,"Target","",16);require(targets.contains(target)&&seen.insert(target).second,"unsupported/duplicate NXTheme target");
        const auto title=text(info,"ThemeName","Imported theme",160),author=text(info,"Author","",160);if(out.name.empty())out.name=title;
        Json evidence={{"target",target},{"name",title},{"author",author},{"version",info.at("Version")}};
        const auto wallpaper=part.contains("image.dds")?"image.dds":part.contains("image.jpg")?"image.jpg":"";
        require(*wallpaper,"NXTheme has no supported wallpaper; layout-only input requires complete firmware reconstruction");auto decoded=image(part.at(wallpaper));
        require(decoded.width==1280&&decoded.height==720,"NXTheme wallpaper must be 1280x720");if(out.preview.empty())out.preview=decoded;
        const auto asset="wallpaper-"+target,path="assets/"+target+".tga";out.files[path]=encode_tga(decoded);images[asset]=path;
        models[target]=screen(target,asset);screens[target]="screens/"+target+".json";
        auto& omissions=out.report["omissions"];
        for(const auto& [name,bytes]:part) {
            const auto lower=folded(name);
            for(auto suffix:{".nca",".nsp",".xci",".nso",".keys",".glsl",".lua",".bflyt",".bflan",".bntx"})require(!lower.ends_with(suffix),"prohibited raw asset in NXTheme: "+name);
            if(name=="info.json"||name==wallpaper||lower=="preview.png")continue;
            if(name=="layout.json"||name=="common.json") {
                const auto patch_json=json(bytes);require(patch_json.is_object(),"invalid layout patch");Files fw;
                try {require(bool(firmware),"user-owned extracted firmware is not provided");const auto fwbytes=firmware(name=="common.json"?"common":target);fw=container(fwbytes);evidence["firmware_identity"]={{"target",name=="common.json"?"common":target},{"sha256",sha256(fwbytes)},{"members",fw.size()},{"firmware_release","not inferred from filenames or FLYT version"}};}
                catch(const std::exception& e){issue(omissions,name,"missing-firmware",e.what());continue;}
                const auto patches=patch_json.value("Files",Json::array());require(patches.is_array()&&patches.size()<=64,"layout Files limit");
                for(const auto& file:patches) {
                    const auto fname=text(file,"FileName","",240);require(runtime_fs::relative_path(fname)&&fname.ends_with(".bflyt"),"invalid firmware layout path");
                    if(!fw.contains(fname)){issue(omissions,fname,"missing-layout","Layout file absent in the supplied firmware");continue;}
                    auto layout=bflyt(fw.at(fname),fname,omissions);patch(layout,file,fname,omissions);const auto boxes=geometry(layout,fname,omissions);
                    Json panes=Json::array(),nodes=Json::array({{{"id","base"},{"type","rect"},{"w",1920},{"h",1080},{"color",0x101e29}}});std::map<std::string,std::string> node_ids;
                    for(std::size_t i=0;i<boxes.size();++i){const auto& g=boxes[i];const auto& p=layout.panes[i];
                        Json pane={{"name",p.name},{"type",p.type},{"x",(layout.width/2+g.cx-g.w*g.sx/2)*1920/layout.width},{"y",(layout.height/2-g.cy-g.h*g.sy/2)*1080/layout.height},{"w",g.w*g.sx*1920/layout.width},{"h",g.h*g.sy*1080/layout.height},{"alpha",g.alpha},{"visible",g.visible}};panes.push_back(pane);
                        if(p.rx||p.ry||p.rz||p.z||g.sx<=0||g.sy<=0)continue;
                        auto node=pane;node.erase("name");node.erase("type");const auto id="pane-"+std::to_string(i);node_ids[p.name]=id;node["id"]=id;node["type"]="rect";node["color"]=0x182f3d;node["edge"]=0x9ed7cf;node["border"]=1;nodes.push_back(node);
                        if(p.type=="pic1")issue(omissions,fname+":"+p.name,"material-pipeline","Material "+std::to_string(p.material)+" is parsed/patched but its texture/blend pipeline is not rendered");
                    }
                    Json materials=Json::array();for(const auto& material:layout.materials)materials.push_back({{"name",material.name},{"flags",material.flags},{"foreground",material.foreground},{"background",material.background},{"references",material.references},{"transforms",material.transforms}});
                    reconstruction.push_back({{"file",fname},{"version",layout.version},{"panes",panes},{"materials",materials}});
                    require(models.size()<14,"too many reconstructed screens for scene budget");const auto sid="fw-"+std::to_string(reconstruction.size());screens[sid]="screens/"+sid+".json";
                    nodes.push_back(label("preview-title",fname+" — geometry preview, not material parity",80,70,1760,28));nodes.push_back(label("preview-footer","L1 / R1: screens · Back: native library · Options: recovery picker",80,1030,1760,24));
                    Json tracks=Json::array();
                    const auto anims=patch_json.value("Anims",Json::array());if(!anims.is_null())require(anims.is_array()&&anims.size()<=64,"layout animation count limit");
                    for(const auto& anim:anims) {
                        const auto anim_name=text(anim,"FileName","",240);require(runtime_fs::relative_path(anim_name)&&anim_name.ends_with(".bflan"),"invalid firmware animation path");
                        // Never guess which of several patched FLYT files owns a pane name.
                        if(patches.size()!=1){issue(omissions,anim_name,"animation-layout","Animation binding is ambiguous across several patched layouts");continue;}
                        Json data;if(anim.contains("AnimJson")&&!anim.at("AnimJson").is_null())data=json(text(anim,"AnimJson","",512*1024));
                        else if(fw.contains(anim_name))data=bflan(fw.at(anim_name));
                        else {issue(omissions,anim_name,"missing-animation","Animation missing in user firmware");continue;}
                        for(const auto& track:animation_tracks(data,layout,boxes,node_ids,anim_name,omissions)) {
                            const auto duplicate=std::find_if(tracks.begin(),tracks.end(),[&](const auto& old){return old.at("node")==track.at("node")&&old.at("property")==track.at("property");});
                            if(duplicate==tracks.end())tracks.push_back(track);else issue(omissions,anim_name,"animation-conflict","Competing tracks need original event scheduling; later track not applied");
                        }
                    }
                    models[sid]={{"nodes",nodes},{"input",{{"back",{{"screen","home"}}}}}};
                    if(!tracks.empty()){const auto aid="motion-"+sid;animations[aid]="animations/"+aid+".json";animation_files[aid]=tracks;out.files["animations/"+aid+".json"]=tracks.dump();models[sid]["animations"]=Json::array({aid});}
                    issue(omissions,fname,"geometry-preview","Patched hierarchy/transforms are runnable on a separate diagnostic screen; original materials, fonts, service bindings are not impersonated");
                }
                for(auto it=patch_json.begin();it!=patch_json.end();++it)if(it.key()!="Files"&&it.key()!="Anims"&&it.key()!="PatchName"&&it.key()!="AuthorName"&&it.key()!="TargetName"&&it.key()!="ID")issue(omissions,name,"layout-metadata",it.key()+" compatibility/application semantics are not emulated");
            } else if((name=="settings.dds"||name=="settings.png") && target=="home") {
                auto icon=image(bytes);require(icon.width==64&&icon.height==56,"settings applet icon must be 64x56");const auto icon_path="assets/settings.tga";
                out.files[icon_path]=encode_tga(icon);images["settings-icon"]=icon_path;
                models[target]["nodes"].push_back({{"id","settings-button"},{"type","focus"},{"x",1660},{"y",80},{"w",110},{"h",110},{"activate",{{"screen","set"}}},{"children",Json::array({{{"id","settings-icon"},{"type","image"},{"asset","settings-icon"},{"x",7},{"y",13},{"w",96},{"h",84}}})}});
                issue(omissions,name,"native-icon-remap","Settings artwork maps to ProsperoEden accessibility/sound controls, not Switch settings parity");
            } else issue(omissions,name,"unmapped-asset","No supported native semantic mapping for this member");
        }
        out.report["parts"].push_back(std::move(evidence));
    }
    if(!models.contains("home")){models["home"]=screen("home",images.begin().key());screens["home"]="screens/home.json";}
    if(!models.contains("set")){models["set"]=screen("set",images.begin().key());screens["set"]="screens/set.json";}
    std::vector<std::string> order;for(auto it=models.begin();it!=models.end();++it)order.push_back(it.key());
    for(std::size_t i=0;i<order.size();++i) {auto& model=models[order[i]];model["input"]["l1"]={{"screen",order[(i+order.size()-1)%order.size()]}};model["input"]["r1"]={{"screen",order[(i+1)%order.size()]}};out.files[screens.at(order[i]).get<std::string>()]=model.dump();}
    Json description={{"schema",1},{"id",out.id},{"name",out.name},{"entry","home"},{"images",images},{"screens",screens},{"animations",animations}};
    (void)parse(description,models,animation_files);out.files["theme.json"]=description.dump();
    out.report["mode"]=reconstruction.empty()?"native-artwork":"native-reconstruction-subset";out.report["reconstruction"]=reconstruction;
    out.report["limitations"]=Json::array({"No firmware, Nintendo artwork, keys or original packages are bundled or redistributed", "Not faithful Switch/reference parity; unsupported items require separate explicit partial-import consent"});
    out.files["import-report.json"]=out.report.dump(2);
    out.files["manifest.json"]=Json{{"schema",1},{"id",out.id},{"name",out.name+" (imported)"},{"api",{{"major",1},{"min_minor",1}}},{"host_contract","launcher-v1.0"},{"state_schema",1},{"required_capabilities",Json::array({"themes.scene.v1","actions.launch.v1"})},{"scene","theme.json"}}.dump();
    std::size_t total=0;for(const auto& [name,bytes]:out.files){total+=bytes.size();require(total<=128*1024*1024,"converted package size limit");}return out;
}
} // namespace pe::ui::scene::nx
