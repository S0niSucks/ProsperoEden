// SPDX-License-Identifier: GPL-3.0-or-later
#include "pe/ui/scene/package.hpp"
#include "pe/ui/scene/player.hpp"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>

using namespace pe::ui::scene;
template<class F> void rejects(F fn) { bool caught = false; try { fn(); } catch (const std::exception&) { caught = true; } assert(caught); }
int main(int argc, char** argv) {
    assert(argc == 2);
    const auto package = load(argv[1]);
    assert(package.model.screens.contains("home"));
    Json description = {{"schema",1},{"id","test"},{"entry","home"},{"screens",{{"home","home.json"}}},{"images",{{"panel","panel.tga"}}}};
    Json screen = {{"focus","games"},{"transition",0.2},{"nodes",Json::array({
        {{"id","background"},{"type","rect"},{"w",1920},{"h",1080}},
        {{"id","mask"},{"type","mask"},{"w",800},{"h",600},{"children",Json::array({
            {{"id","panel"},{"type","nineslice"},{"asset","panel"},{"w",800},{"h",600},{"slice",{0.1,0.1,0.1,0.1}}}
        })}},
        {{"id","games"},{"type","grid"},{"w",1000},{"h",800},{"rows",2},{"columns",3},{"source","/library/games"},
         {"activate",{{"action","launch.request"},{"args",{{"game",{{"$bind","/selected/handle"}}}}}}},
         {"children",Json::array({{{"id","title"},{"type","text"},{"bind","/item/name"},{"w",220},{"y",28}}})}},
        {{"id","motes"},{"type","particles"},{"w",1900},{"h",1000},{"count",12}}
    })},{"animations",{"enter"}}};
    Json animations = Json::parse(R"({"enter":[{"node":"background","property":"x","duration":1,"loop":true,"keys":[{"at":0,"value":0},{"at":1,"value":100}]}]})");
    auto model = parse(description,{{"home",screen}},animations);
    Player player(model);
    Json games=Json::array(); for(int i=0;i<100;++i) games.push_back({{"handle","game-"+std::to_string(i)},{"name","Game "+std::to_string(i)}});
    const auto query=[&](const std::string& q) -> Json { return q=="library"?Json{{"games",games}}:Json::object(); };
    std::string requested; int sound_count=0;
    const auto dispatch=[&](const std::string& name,const Json& args) { if(name=="launch.request") requested=args.at("game"); };
    const auto sound=[&](const auto&,bool) { ++sound_count; };
    for(int i=0;i<12;++i) player.press("right",query,dispatch,sound);
    assert(player.selected_index()==12);
    player.press("select",query,dispatch,sound); assert(requested=="game-12");
    games.erase(games.begin());
    player.press("select",query,dispatch,sound); assert(requested=="game-12" && player.selected_index()==11);
    int commands=0, titles=0, slices=0, particles=0, clips=0, transforms=0, opacity=0;
    float background_x=-1;
    auto draw=[&](const std::string& op,const Json& p) {
        ++commands; assert(p.is_object());
        if(op=="text") { ++titles; assert(p.at("text").get<std::string>().starts_with("Game ")); }
        if(op=="image") ++slices;
        if(op=="circle") ++particles;
        if(op=="rect" && p.value("id","")=="background") background_x=p.value("x",0.0f);
        if(op=="clip") ++clips;
        if(op=="unclip") --clips;
        if(op=="transform") ++transforms;
        if(op=="untransform") --transforms;
        if(op=="opacity") ++opacity;
        if(op=="unopacity") --opacity;
    };
    player.update(0.05f,false); player.draw(query,draw,sound);
    assert(titles==6 && slices==9 && particles==12 && commands<250 && clips==0 && transforms==0 && opacity==0 && background_x>0 && background_x<100);
    particles=0; player.update(0,true); player.draw(query,draw,sound); assert(particles==0 && background_x==100);
    // A generation change which removes the selected item must clamp rather than retain a stale action handle.
    games=Json::array(); player.press("select",query,dispatch,sound); assert(requested=="game-12");
    auto bad=screen; bad["nodes"][0]["shader"]="unsafe"; rejects([&]{parse(description,{{"home",bad}},animations);});
    bad=screen; bad["nodes"][0]["alpha"]=-1; rejects([&]{parse(description,{{"home",bad}},animations);});
    bad=screen; bad["nodes"][2]["nav"]={{"right","absent"}}; rejects([&]{parse(description,{{"home",bad}},animations);});
    bad=screen; bad["nodes"][2]["rows"]=64; rejects([&]{parse(description,{{"home",bad}},animations);});
    bad=screen; bad["nodes"][2]["activate"]["args"]["confirmed"]=true; rejects([&]{parse(description,{{"home",bad}},animations);});
    rejects([]{binding("/filesystem/keys");}); rejects([]{validate_font("bad");});
    rejects([]{tga(std::string(18,'\0'),1024);});
    std::string image(22,'\0'); image[2]=2; image[12]=1; image[14]=1; image[16]=32; image[17]=40;
    image[18]=3; image[19]=2; image[20]=1; image[21]=127;
    const auto decoded=tga(image,4); assert(decoded.width==1 && decoded.rgba==std::vector<std::uint8_t>({1,2,3,127}));
    rejects([&]{tga(image,3);});
    namespace ff=pe::gfx::font_format;
    ff::Header h{ff::kMagic,ff::kVersion,2,2,24,4,18,-6,0,1,0};
    ff::Glyph glyph{63,0,0,1,1,0,0,12};
    std::string font(sizeof(h)+sizeof(glyph)+4,'\0'); std::memcpy(font.data(),&h,sizeof(h)); std::memcpy(font.data()+sizeof(h),&glyph,sizeof(glyph));
    validate_font(font);
    h.atlas_width=65535; std::memcpy(font.data(),&h,sizeof(h)); rejects([&]{validate_font(font);});
    std::string recording="RIFF"; recording.append(4,'\0'); recording+="WAVEfmt ";
    const auto u16=[&](std::uint32_t v) { recording.push_back(static_cast<char>(v));recording.push_back(static_cast<char>(v>>8)); };
    const auto u32=[&](std::uint32_t v) { u16(v);u16(v>>16); };
    u32(16);u16(1);u16(2);u32(48000);u32(192000);u16(4);u16(16);recording+="data";u32(16);recording.append(16,'\0');
    const auto riff_size=static_cast<std::uint32_t>(recording.size()-8); for(int i=0;i<4;++i) recording[4+i]=static_cast<char>(riff_size>>(i*8));
    assert(wav(recording,32).frames==4); rejects([&]{wav(recording,31);});
    recording[20]=3; rejects([&]{wav(recording,32);});
    Json verbose=Json::array();
    for(int i=0;i<12;++i) verbose.push_back({{"id","label"+std::to_string(i)},{"type","text"},{"text",std::string(4096,'x')}});
    Player text_budget(parse(description,{{"home",{{"nodes",verbose}}}}));
    rejects([&]{text_budget.draw(query,[](const auto&,const auto&){},sound);});
    const auto temp=std::filesystem::temp_directory_path()/"pe-scene-reader-test";
    std::filesystem::create_directories(temp);
    {std::ofstream f(temp/"duplicate.json");f<<"{\"id\":1,\"id\":2}";}
    {std::ofstream f(temp/"deep.json");f<<std::string(100,'[')<<std::string(100,']');}
    std::filesystem::create_symlink(temp/"duplicate.json",temp/"link.json");
    Reader reader(temp.string());
    rejects([&]{reader.json("duplicate.json");}); rejects([&]{reader.json("deep.json");});
    rejects([&]{reader.json("link.json");}); rejects([&]{reader.json("../outside");});
    std::filesystem::remove(temp/"link.json");std::filesystem::remove(temp/"duplicate.json");std::filesystem::remove(temp/"deep.json");std::filesystem::remove(temp);
    // Deterministic mutation smoke-fuzz: parser and image/font readers only, no external assets.
    std::mt19937 rng(239173);
    for(int i=0;i<2000;++i) {
        auto mutated=screen; const char* keys[]{"x","alpha","rows","type","children","source","nav"};
        mutated["nodes"][rng()%4][keys[rng()%7]]=rng()%2?Json(rng()):Json(std::string(rng()%100,'x'));
        try { (void)parse(description,{{"home",mutated}},animations); } catch(const std::exception&) {}
        std::string bytes(rng()%256,'\0'); for(auto& c:bytes)c=static_cast<char>(rng());
        try { (void)tga(bytes,1024*1024); } catch(const std::exception&) {}
        try { validate_font(bytes); } catch(const std::exception&) {}
    }
    std::cout<<"scene parser/controller/projection/resource tests passed (2000 mutations)\n";
}
