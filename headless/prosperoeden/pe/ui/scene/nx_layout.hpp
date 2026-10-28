// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "pe/ui/scene/nx_archive.hpp"
// Independent format reader. Format provenance: SwitchThemeInjector 66ccc56, BFLYT section
// field definitions and LayoutPatches JSON schema; no upstream implementation is embedded.
namespace pe::ui::scene::nx {
inline void issue(Json& report,const std::string& path,const std::string& code,const std::string& detail) {
    require(report.size()<256,"NX diagnostic limit exceeded");report.push_back({{"path",path},{"code",code},{"detail",detail}});
}
struct Pane {
    std::string name,type;int parent=-1;float x{},y{},z{},rx{},ry{},rz{},sx=1,sy=1,w{},h{},alpha=1;
    bool visible=true,uniform_color=true;unsigned ox{},oy{},px{},py{},material{};std::uint32_t rgba=0xffffffff;
};
struct Material {
    std::string name;std::uint32_t flags{},foreground{},background{};
    Json references=Json::array(),transforms=Json::array();
};
struct Layout {
    std::vector<Pane> panes;std::vector<std::string> textures;std::vector<Material> materials;float width=1280,height=720;std::uint32_t version{};
    Pane& find(const std::string& name){const auto it=std::find_if(panes.begin(),panes.end(),[&](const auto& p){return p.name==name;});require(it!=panes.end(),"firmware pane missing: "+name);return *it;}
    const Pane& find(const std::string& name) const {const auto it=std::find_if(panes.begin(),panes.end(),[&](const auto& p){return p.name==name;});require(it!=panes.end(),"firmware pane missing: "+name);return *it;}
};
inline Layout bflyt(std::string_view bytes,const std::string& path,Json& omissions) {
    Binary b{bytes};require(b.view(0,4)=="FLYT","expected FLYT: "+path);const auto bom=b.view(4,2);
    require(bom==std::string_view("\xff\xfe",2)||bom==std::string_view("\xfe\xff",2),"invalid FLYT endian marker");b.big=bom==std::string_view("\xfe\xff",2);
    Layout out;out.version=b.u32(8);require(b.u32(12)==bytes.size() && b.u16(6)>=20,"invalid FLYT header");
    const auto count=b.u16(16);require(count<=1024,"FLYT section count exceeded");std::size_t at=b.u16(6);std::vector<int> parents;int last=-1;std::set<std::string> names;
    for(unsigned i=0;i<count;++i) {
        const auto type=b.view(at,4);const auto size=b.u32(at+4);require(size>=8,"invalid FLYT section size");b.view(at,size);Binary s{b.view(at,size),b.big};
        if(type=="pas1") {require(last>=0 && parents.size()<12,"invalid FLYT parent stack");parents.push_back(last);}
        else if(type=="pae1") {require(!parents.empty(),"unbalanced FLYT parent stack");parents.pop_back();}
        else if(type=="lyt1") {out.width=s.f32(12);out.height=s.f32(16);require(out.width>0&&out.height>0,"invalid FLYT canvas");}
        else if(type=="txl1") {
            const auto n=s.u16(8);require(n<=256,"FLYT texture count limit");s.view(12,n*4);
            for(unsigned t=0;t<n;++t){const auto pos=12+s.u32(12+t*4);require(pos<size,"FLYT texture offset outside section");out.textures.push_back(s.name(pos,std::min<unsigned>(241,size-pos)));}
        }
        else if(type=="mat1") {
            const auto n=s.u16(8);require(n<=256,"FLYT material count limit");s.view(12,n*4);
            const auto rgba=[&](std::size_t pos){return (s.u8(pos)<<24)|(s.u8(pos+1)<<16)|(s.u8(pos+2)<<8)|s.u8(pos+3);};
            for(unsigned m=0;m<n;++m) {
                const auto pos=s.u32(12+m*4),end=m+1<n?s.u32(16+m*4):size;require(pos>=12+n*4 && end>=pos && end<=size,"invalid FLYT material extent");
                Binary material{s.view(pos,end-pos),s.big};Material value;value.name=material.name(0,28);
                const auto modern=out.version>=0x08000000;value.flags=material.u32(modern?28:36);value.foreground=rgba(pos+(modern?36:28));value.background=rgba(pos+(modern?40:32));
                unsigned cursor=modern?44:40;
                for(unsigned t=0;t<(value.flags&3);++t,cursor+=4)value.references.push_back({{"index",material.u16(cursor)},{"WrapS",material.u8(cursor+2)},{"WrapT",material.u8(cursor+3)}});
                for(unsigned t=0;t<((value.flags>>2)&3);++t,cursor+=20)value.transforms.push_back({{"X",material.f32(cursor)},{"Y",material.f32(cursor+4)},{"Rotation",material.f32(cursor+8)},{"ScaleX",material.f32(cursor+12)},{"ScaleY",material.f32(cursor+16)}});
                out.materials.push_back(std::move(value));
            }
        }
        else if(type=="pan1"||type=="pic1"||type=="txt1"||type=="bnd1"||type=="wnd1"||type=="prt1") {
            require(out.panes.size()<256,"FLYT pane limit");s.view(0,84);Pane p;p.name=s.name(12,24);p.type=type;
            require(!p.name.empty()&&names.insert(p.name).second,"duplicate FLYT pane name");p.parent=parents.empty()?-1:parents.back();
            const auto origin=s.u8(9);p.ox=(origin>>6)&3;p.oy=(origin>>4)&3;p.px=(origin>>2)&3;p.py=origin&3;
            require(p.ox<=2&&p.oy<=2&&p.px<=2&&p.py<=2,"unsupported FLYT origin");p.visible=s.u8(8)&1;p.alpha=float(s.u8(10))/255;
            p.x=s.f32(44);p.y=s.f32(48);p.z=s.f32(52);p.rx=s.f32(56);p.ry=s.f32(60);p.rz=s.f32(64);p.sx=s.f32(68);p.sy=s.f32(72);p.w=s.f32(76);p.h=s.f32(80);
            require(p.w>=0&&p.h>=0,"negative FLYT size");
            if(type=="pic1") {
                p.rgba=(s.u8(84)<<24)|(s.u8(85)<<16)|(s.u8(86)<<8)|s.u8(87);
                for(unsigned c=1;c<4;++c)if(s.view(84+c*4,4)!=s.view(84,4)){p.uniform_color=false;issue(omissions,path+":"+p.name,"vertex-gradient","Four-corner colors require a supported material renderer");break;}
                p.material=s.u16(100);
            }
            if(type=="prt1"||type=="wnd1"||type=="txt1")issue(omissions,path+":"+p.name,"pane-type","Unconverted "+std::string(type)+" content; geometry is retained for patches");
            out.panes.push_back(std::move(p));last=int(out.panes.size())-1;
        }
        else if(type!="txl1"&&type!="mat1"&&type!="fnl1"&&type!="usd1"&&type!="grp1"&&type!="grs1"&&type!="gre1")issue(omissions,path,"section","Unconverted FLYT section "+std::string(type));
        at+=size;
    }
    require(parents.empty() && at==bytes.size(),"FLYT extent/parent mismatch");
    for(auto& material:out.materials)for(auto& reference:material.references){const auto index=reference.at("index").get<unsigned>();require(index<out.textures.size(),"material texture index outside table");reference["Name"]=out.textures[index];}
    return out;
}
inline std::uint32_t rgba_hex(const std::string& value) {
    require(value.size()==8 && value.find_first_not_of("0123456789abcdefABCDEF")==value.npos,"expected RRGGBBAA color");return std::stoul(value,nullptr,16);
}
inline float component(const Json& v,const char* key,float fallback) {return number(v,key,fallback,-8192,8192);}
inline void patch(Layout& layout,const Json& file,const std::string& path,Json& omissions) {
    fields(file,{"FileName","Patches","Materials","AddGroups","PushBackPanes","PullFrontPanes"});
    const auto patches=file.value("Patches",Json::array());require(patches.is_array()&&patches.size()<=256,"invalid pane patches");
    for(const auto& p:patches) {
        fields(p,{"PaneName","Position","Rotation","Scale","Size","Visible","OriginX","OriginY","ParentOriginX","ParentOriginY","ColorTL","ColorTR","ColorBL","ColorBR","UsdPatches"});
        auto& pane=layout.find(text(p,"PaneName","",24));
        for(auto key:{"Position","Rotation","Scale","Size"})if(p.contains(key)&&!p.at(key).is_null()) {
            const auto& v=p.at(key);fields(v,{"X","Y","Z"});const auto x=component(v,"X",0),y=component(v,"Y",0),z=component(v,"Z",0);
            if(std::string_view(key)=="Position"){pane.x=x;pane.y=y;pane.z=z;}
            if(std::string_view(key)=="Rotation"){pane.rx=x;pane.ry=y;pane.rz=z;}
            if(std::string_view(key)=="Scale"){pane.sx=x;pane.sy=y;}
            if(std::string_view(key)=="Size"){require(x>=0&&y>=0,"invalid patched size");pane.w=x;pane.h=y;}
        }
        if(p.contains("Visible")&&!p.at("Visible").is_null())pane.visible=flag(p,"Visible");
        for(auto [key,target]:std::initializer_list<std::pair<const char*,unsigned*>>{{"OriginX",&pane.ox},{"OriginY",&pane.oy},{"ParentOriginX",&pane.px},{"ParentOriginY",&pane.py}})
            if(p.contains(key)&&!p.at(key).is_null()){require(p.at(key).is_number_integer()&&p.at(key).get<int>()>=0&&p.at(key).get<int>()<=2,"invalid patched origin");*target=p.at(key);}
        for(auto key:{"ColorTL","ColorTR","ColorBL","ColorBR","UsdPatches"})if(p.contains(key)&&!p.at(key).is_null())issue(omissions,path+":"+pane.name,"pane-property",std::string(key)+" cannot yet be rendered exactly");
    }
    if(file.contains("Materials")&&!file.at("Materials").is_null()) {
        const auto& materials=file.at("Materials");require(materials.is_array()&&materials.size()<=256,"material patch count limit");
        for(const auto& patch:materials) {
            fields(patch,{"MaterialName","ForegroundColor","BackgroundColor","Refs","Transforms"});const auto name=text(patch,"MaterialName","",28);
            auto it=std::find_if(layout.materials.begin(),layout.materials.end(),[&](const auto& m){return m.name==name;});require(it!=layout.materials.end(),"firmware material missing: "+name);
            if(patch.contains("ForegroundColor")&&!patch.at("ForegroundColor").is_null())it->foreground=rgba_hex(text(patch,"ForegroundColor"));
            if(patch.contains("BackgroundColor")&&!patch.at("BackgroundColor").is_null())it->background=rgba_hex(text(patch,"BackgroundColor"));
            for(auto kind:{"Refs","Transforms"})if(patch.contains(kind)&&!patch.at(kind).is_null()) {
                const auto& edits=patch.at(kind);require(edits.is_array()&&edits.size()<=3,"material transform/reference count limit");
                for(const auto& edit:edits) {
                    const auto texture=text(edit,"Name","",240);auto ref=std::find_if(it->references.begin(),it->references.end(),[&](const auto& r){return r.at("Name").template get<std::string>()==texture;});
                    require(ref!=it->references.end(),"material texture reference missing: "+texture);const auto index=static_cast<std::size_t>(ref-it->references.begin());
                    if(std::string_view(kind)=="Refs") {
                        fields(edit,{"Name","WrapS","WrapT"});for(auto key:{"WrapS","WrapT"})if(edit.contains(key)&&!edit.at(key).is_null()){require(edit.at(key).is_number_integer()&&edit.at(key).get<int>()>=0&&edit.at(key).get<int>()<=7,"invalid material wrap");(*ref)[key]=edit.at(key);}
                    } else {
                        fields(edit,{"Name","X","Y","Rotation","ScaleX","ScaleY"});require(index<it->transforms.size(),"material texture transform missing");
                        for(auto key:{"X","Y","Rotation","ScaleX","ScaleY"})if(edit.contains(key)&&!edit.at(key).is_null())it->transforms[index][key]=number(edit,key,0,-8192,8192);
                    }
                }
            }
        }
    }
    for(auto key:{"AddGroups","PushBackPanes","PullFrontPanes"})if(file.contains(key)&&!file.at(key).is_null()&&!file.at(key).empty())issue(omissions,path,"layout-operation",std::string(key)+" is not yet converted");
}
struct Geometry {float cx{},cy{},sx=1,sy=1,w{},h{},alpha=1;bool visible=true;};
inline std::vector<Geometry> geometry(const Layout& layout,const std::string& path,Json& omissions) {
    std::vector<Geometry> out;out.reserve(layout.panes.size());
    for(const auto& p:layout.panes) {
        Geometry parent;if(p.parent>=0)parent=out.at(p.parent);else {parent.w=layout.width;parent.h=layout.height;}
        Geometry g;g.sx=parent.sx*p.sx;g.sy=parent.sy*p.sy;g.w=p.w;g.h=p.h;g.alpha=parent.alpha*p.alpha;g.visible=parent.visible&&p.visible&&g.sx!=0&&g.sy!=0;
        const auto anchor=[](unsigned origin,float extent){return origin==1?-extent/2:origin==2?extent/2:0;};
        g.cx=parent.cx+(p.x+anchor(p.px,parent.w))*parent.sx-anchor(p.ox,p.w)*g.sx;
        // OriginY Top is +Y in Nintendo layout space; host +Y points down.
        g.cy=parent.cy+(p.y-anchor(p.py,parent.h))*parent.sy+anchor(p.oy,p.h)*g.sy;
        if(p.rx!=0||p.ry!=0||p.rz!=0||p.z!=0||g.sx<0||g.sy<0)issue(omissions,path+":"+p.name,"transform","3D/rotated/mirrored panes are not supported by the retained host renderer");
        out.push_back(g);
    }return out;
}
} // namespace pe::ui::scene::nx
