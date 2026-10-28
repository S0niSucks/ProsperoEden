// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "pe/ui/scene/nx_layout.hpp"
namespace pe::ui::scene::nx {
// Both binary BFLAN and SwitchThemeInjector's AnimJson are normalized into the same bounded
// field representation. Integer keys are held; floating keys use their per-frame Hermite slope.
inline Json bflan(std::string_view bytes) {
    Binary b{bytes};require(b.view(0,4)=="FLAN","expected FLAN");const auto bom=b.view(4,2);
    require(bom==std::string_view("\xff\xfe",2)||bom==std::string_view("\xfe\xff",2),"invalid FLAN byte order");b.big=bom==std::string_view("\xfe\xff",2);
    require(b.u16(6)==20&&b.u32(12)==bytes.size()&&b.u16(16)<=32,"invalid FLAN header");Json result;std::size_t at=20;unsigned total_tracks=0,total_keys=0;
    for(unsigned section=0;section<b.u16(16);++section) {
        const auto type=b.view(at,4);const auto size=b.u32(at+4);require(size>=8,"invalid FLAN section extent");Binary s{b.view(at,size),b.big};
        if(type=="pai1") {
            require(result.is_null(),"duplicate FLAN pai1");const auto textures=s.u16(12),count=s.u16(14),table=s.u32(16);require(textures<=256&&count<=128,"FLAN entry budget");s.view(table,count*4);
            Json entries=Json::array();
            for(unsigned i=0;i<count;++i) {
                const auto entry=s.u32(table+i*4),tags=s.u8(entry+28),target=s.u8(entry+29);require(tags<=32&&target<=2,"FLAN tag budget/target");s.view(entry+32,tags*4);Json tag_list=Json::array();
                for(unsigned j=0;j<tags;++j) {
                    const auto tag=entry+s.u32(entry+32+j*4)+(target==2?4:0),tracks=s.u32(tag+4);require(tracks<=32,"FLAN track budget");s.view(tag+8,tracks*4);Json track_list=Json::array();
                    for(unsigned k=0;k<tracks;++k) {
                        const auto track=tag+s.u32(tag+8+k*4),kind=s.u16(track+2),n=s.u16(track+4),data=track+s.u32(track+8);
                        require((kind==1||kind==2)&&n>=1&&n<=64,"unsupported FLAN key type/count");s.view(data,n*(kind==1?8:12));Json keys=Json::array();
                        require(++total_tracks<=128 && (total_keys+=n)<=8192,"FLAN aggregate track/key budget exceeded");
                        for(unsigned f=0;f<n;++f){const auto key=data+f*(kind==1?8:12);keys.push_back({{"Frame",s.f32(key)},{"Value",kind==2?s.f32(key+4):float(static_cast<std::int16_t>(s.u16(key+4)))},{"Blend",kind==2?s.f32(key+8):float(static_cast<std::int16_t>(s.u16(key+6)))}});}
                        track_list.push_back({{"Index",s.u8(track)},{"AnimationTarget",s.u8(track+1)},{"DataType",kind},{"KeyFrames",keys}});
                    }
                    tag_list.push_back({{"TagType",std::string(s.view(tag,4))},{"Entries",track_list}});
                }
                entries.push_back({{"Name",s.name(entry,28)},{"Target",target},{"Tags",tag_list}});
            }
            result={{"pai1",{{"FrameSize",s.u16(8)},{"Flags",s.u8(10)},{"Textures",Json::array()},{"Entries",entries}}}};
            if(textures)result["texture_patterns_present"]=true;
        } else require(type=="pat1","unsupported FLAN section");
        at+=size;
    }
    require(at==bytes.size()&&!result.is_null(),"FLAN extent or missing pai1");return result;
}
inline Json animation_tracks(const Json& animation,const Layout& layout,const std::vector<Geometry>& boxes,const std::map<std::string,std::string>& node_ids,const std::string& path,Json& omissions) {
    const auto& pai=animation.at("pai1");const float frames=number(pai,"FrameSize",0,1,3600);const auto entries=pai.value("Entries",Json::array());require(entries.is_array()&&entries.size()<=128,"FLAN JSON entry budget");
    Json tracks=Json::array();
    for(const auto& entry:entries) {
        const auto name=text(entry,"Name","",28);const auto found=node_ids.find(name);
        if(found==node_ids.end()){issue(omissions,path+":"+name,"animation-pane","No retained geometry for this animation target");continue;}
        const auto& pane=layout.find(name);const auto index=static_cast<std::size_t>(&pane-layout.panes.data());const auto& box=boxes.at(index);
        const auto tags=entry.value("Tags",Json::array());require(tags.is_array()&&tags.size()<=32,"FLAN JSON tag budget");
        for(const auto& tag:tags) {
            const auto kind=text(tag,"TagType","",4);const auto list=tag.value("Entries",Json::array());require(list.is_array()&&list.size()<=32,"FLAN JSON track budget");
            for(const auto& track:list) {
                const auto target=track.value("AnimationTarget",-1),datatype=track.value("DataType",0);
                if(entry.value("Target",-1)!=0||kind!="FLPA"||target<0||target>1||track.value("Index",0)!=0||(datatype!=1&&datatype!=2)) {
                    issue(omissions,path+":"+name,"animation-track","Unconverted "+kind+" target "+std::to_string(target));continue;
                }
                const bool has_children=std::any_of(layout.panes.begin(),layout.panes.end(),[&](const auto& p){return p.parent==int(index);});
                if(has_children){issue(omissions,path+":"+name,"animation-hierarchy","Animated parents need descendant matrix composition; not flattened incorrectly");continue;}
                const auto& keys=track.at("KeyFrames");require(keys.is_array()&&!keys.empty()&&keys.size()<=64,"FLAN JSON key budget");Json converted=Json::array();float previous=-1;
                const float parent_scale=pane.parent>=0?(target==0?boxes.at(pane.parent).sx:boxes.at(pane.parent).sy):1;
                const float factor=parent_scale*(target==0?1920/layout.width:-1080/layout.height);
                const float base=target==0?(layout.width/2+box.cx-box.w*box.sx/2)*1920/layout.width:(layout.height/2-box.cy-box.h*box.sy/2)*1080/layout.height;
                for(const auto& key:keys) {const float frame=number(key,"Frame",0,0,frames);require(frame>previous,"FLAN keys must be ordered");previous=frame;
                    converted.push_back({{"at",frame/frames},{"value",base+number(key,"Value",0,-8192,8192)*factor},{"tangent",number(key,"Blend",0,-1000,1000)*factor*frames}});}
                if(converted.front().at("at")!=0){auto key=converted.front();key["at"]=0;key["tangent"]=0;converted.insert(converted.begin(),key);}
                if(converted.back().at("at")!=1){auto key=converted.back();key["at"]=1;key["tangent"]=0;converted.push_back(key);}
                require(tracks.size()<128,"converted animation budget");tracks.push_back({{"node",found->second},{"property",target==0?"x":"y"},{"duration",frames/60},{"loop",(pai.value("Flags",0)&1)!=0},{"interpolation",datatype==1?"step":"hermite"},{"keys",converted}});
            }
        }
    }
    if(!tracks.empty())issue(omissions,path,"animation-lifecycle","Supported translation curves play on geometry-preview entry at 60 fps; qlaunch event/group scheduling is not emulated");
    return tracks;
}
} // namespace pe::ui::scene::nx
