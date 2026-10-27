// SPDX-License-Identifier: GPL-3.0-or-later
// Included in RuntimeLauncher::Impl; keeps scene integration out of the library/gfx renderers.
Json scene_confirmation;
std::string scene_notice;
bool scene_launch{};
int scene_sync_choice{};

void load_scene_textures(Pack& p) {
    for(const auto& [name,image]:p.scene_resources->images) {
        const auto texture=batch.create_texture(image.width,image.height,image.rgba.data());
        scene::require(texture!=0,"texture upload failed"); p.textures[name]=texture;
    }
    if(!p.scene_resources->font.empty()) {
        p.font_texture=batch.create_font_texture(p.font);
        scene::require(p.font_texture!=0,"font upload failed");
    }
}
void prepare_scene(Pack& p) {
    scene::require(!p.manifest.contains("entry") && !p.manifest.contains("loading_scene"),"scene packs cannot supply scripts/loading code");
    scene::require(p.manifest.value("assets",Json::object()).empty() && p.manifest.value("sounds",std::string{}).empty(),"scene resources belong in theme.json");
    scene::require(p.manifest.contains("required_capabilities") &&
        std::find(p.manifest.at("required_capabilities").begin(),p.manifest.at("required_capabilities").end(),"themes.scene.v1")!=p.manifest.at("required_capabilities").end(),"scene capability must be declared");
    p.scene_resources=std::make_unique<scene::Package>(scene::load(p.folder,p.manifest.at("scene")));
    scene::require(p.scene_resources->model.id==p.id,"theme ID does not match pack");
    if(!p.scene_resources->font.empty()) {
        scene::require(p.font.load(p.scene_resources->font),"invalid theme font"); if(configure_font) configure_font(p.font);
    }
    for(auto& [name,wav]:p.scene_resources->audio) {
        p.clips[name]={wav->samples.data(),wav->samples.size()/2}; p.recordings[name]=std::move(wav);
    }
    p.scene_resources->audio.clear();
    p.scene_player=std::make_unique<scene::Player>(p.scene_resources->model);
    load_scene_textures(p);
}
void scene_sound(Pack& p,const std::string& name,bool music) {
    if(music) {
        if(p.scene_music==name) return;
        if(mixer) mixer->stop_all();
        p.scene_music=name; p.scene_music_left=0;
    }
    if(name.empty() || !mixer) return;
    const auto clip=p.clips.find(name); if(clip==p.clips.end()) return;
    // Both cues and the theme's music obey the existing menu-volume bus, including Off.
    audio::PlayParams params; params.bus=audio::Bus::ui;
    if(mixer->play_clip(&clip->second,params) && music)
        p.scene_music_left=float(clip->second.frames)/float(audio::kSampleRate);
}
void scene_dispatch(Pack& p,const std::string& name,Json args) {
    scene::require(scene::actions.contains(name),"unsupported action");
    args.erase("confirmed");
    static const std::set<std::string> direct{"launcher.picker","launcher.select","library.refresh","profile.choose","launch.request","launch.cancel","sync.cancel"};
    if(!direct.contains(name)) {
        // A theme can request but never draw or answer this host-owned confirmation.
        if(name=="profile.remove" || name=="profile.rename") {
            const int index=args.at("index"); const auto profiles=services.profiles();
            scene::require(index>=0 && index<static_cast<int>(profiles.size()),"invalid profile");
            args["_profile_handle"]=services.profile_key(index);
        }
        scene_confirmation={{"action",name},{"args",std::move(args)}}; return;
    }
    const auto result=action(p,name,args);
    if(!result.value("ok",false)) scene_notice=result.value("message","Operation failed");
    if(name=="launch.request" && result.value("ok",false)) { scene_launch=true; scene_sync_choice=0; }
    if(name=="launch.cancel") scene_launch=false;
}
void scene_emit(Pack& p,const std::string& kind,Json args) {
    if(kind=="focus") {
        scene::require(drawing && ++draw_commands<=8192,"drawing budget exceeded");
        const auto& t=th(); drawing->bordered_rect(rect(args),args.value("radius",t.radius_card),t.row_focus.with_alpha(0.35f),look().high_contrast?4.0f:3.0f,t.accent_pale);
    } else if(kind=="text") {
        scene::require(drawing && ++draw_commands<=8192,"drawing budget exceeded");
        auto text=args.value("text",std::string{}); if(args.value("translate",false)) text=tr(text.c_str());
        if(text.size()>4096) text.resize(4096);
        const auto& font=p.scene_resources->font.empty()?*fonts.font:p.font;
        const auto texture=p.scene_resources->font.empty()?fonts.texture:p.font_texture;
        const auto size=text_size(args.value("size",24.0f)); const auto width=args.value("width",0.0f);
        if(width>0) text=font.fit(text,size,width);
        const auto align=args.value("align",std::string{"left"});
        const auto tint=look().high_contrast?th().text:args.contains("color")?color(args):th().text;
        if(look().high_contrast) {
            const float left=align=="center"?args.value("x",0.0f)-width/2:align=="right"?args.value("x",0.0f)-width:args.value("x",0.0f);
            drawing->rounded_rect({left-4,args.value("y",0.0f)-size-4,std::max(8.0f,width)+8,size*1.5f+8},0,th().base);
        }
        drawing->text(font,texture,text,args.value("x",0.0f),args.value("y",0.0f),size,tint,
            align=="center"?gfx::Align::center:align=="right"?gfx::Align::right:gfx::Align::left);
    } else if(kind=="cover" && args.value("game",std::string{}).empty()) {
        args["color"]=0; draw_command(p,"rect",args);
    } else {
        // Accessibility cannot be overridden by a package's colors or opacity tracks.
        if(look().high_contrast && (kind=="rect" || kind=="circle" || kind=="triangle")) {
            args["color"]=0; args["alpha"]=1;
        }
        if(look().high_contrast && kind=="opacity") args["value"]=1;
        draw_command(p,kind,args);
    }
}
bool scene_dialog_input(Pack& p,const std::string& key) {
    if(!scene_confirmation.is_null()) {
        if(key=="back") scene_confirmation=nullptr;
        else if(key=="select") {
            auto pending=std::move(scene_confirmation); scene_confirmation=nullptr;
            const std::string name=pending.at("action"); auto args=pending.at("args");
            if(args.contains("_profile_handle")) {
                int index=-1; const auto profiles=services.profiles();
                for(int i=0;i<static_cast<int>(profiles.size());++i) if(services.profile_key(i)==args.at("_profile_handle").get<std::string>()) index=i;
                scene::require(index>=0,"profile changed while confirming"); args.erase("_profile_handle"); args["index"]=index;
            }
            args["confirmed"]=true; const auto result=action(p,name,args);
            if(!result.value("ok",false)) scene_notice=result.value("message","Operation failed");
        }
        return true;
    }
    if(!scene_notice.empty()) { if(key=="back" || key=="select") scene_notice.clear(); return true; }
    if(scene_launch && launch.value("stage",std::string{})=="syncing") {
        const auto status=services.save_sync();
        if(key=="back") { action(p,"launch.cancel",Json::object()); scene_launch=false; }
        else if(key=="up" || key=="down") scene_sync_choice=std::clamp(scene_sync_choice+(key=="down"?1:-1),0,2);
        else if(key=="select") {
            if(status.stage==SaveSyncStage::conflict) {
                constexpr const char* choices[]{"console","server","neither"}; action(p,"sync.answer",{{"choice",choices[scene_sync_choice]}});
            } else if(status.stage==SaveSyncStage::failed) action(p,"sync.answer",{{"choice",scene_sync_choice==0?"retry":"play_anyway"}});
        }
        return true;
    }
    return false;
}
void update_scene_launch(Pack& p) {
    if(!scene_launch || launch.value("stage",std::string{})!="ready") return;
    try { action(p,"launch.commit",Json::object()); scene_launch=false; }
    catch(const std::exception& e) { scene_notice=e.what(); scene_launch=false; action(p,"launch.cancel",Json::object()); }
}
void draw_scene_dialog(gfx::DrawList& list) {
    if(scene_confirmation.is_null() && scene_notice.empty() && !scene_launch) return;
    list.reset_state(); const auto& t=th();
    list.rounded_rect({0,0,1920,1080},0,t.base.with_alpha(0.97f));
    auto line=[&](const std::string& s,float y,float size=30.0f) {
        list.text(*fonts.font,fonts.texture,fonts.font->fit(tr(s.c_str()),size,1360),280,y,size,t.text);
    };
    if(!scene_confirmation.is_null()) {
        line("Confirm launcher action",300,44);
        line(scene_confirmation.value("action",std::string{}),400);
        // Identify the affected game/profile using the host snapshot, never theme-supplied labels.
        const auto& args=scene_confirmation.at("args");
        if(args.contains("game")) { const auto* g=catalog.find(args.value("game",std::string{})); if(g) line(g->name,480); }
        if(args.contains("_profile_handle")) {
            const auto profiles=services.profiles();
            for(int i=0;i<static_cast<int>(profiles.size());++i) if(services.profile_key(i)==args.at("_profile_handle").get<std::string>()) line(profiles[i].name,480);
        }
        line("This action may change or remove your data.",580);
    } else if(!scene_notice.empty()) { line("Launcher action failed",300,44); line(scene_notice,430); }
    else {
        line("Preparing game",300,44);
        const auto status=services.save_sync();
        if(status.stage==SaveSyncStage::conflict) {
            line("Choose which save data to use",410);
            constexpr const char* choices[]{"Use console save","Use server save","Keep both unchanged"};
            for(int i=0;i<3;++i) { if(i==scene_sync_choice) list.bordered_rect({260,465+float(i)*75,1400,65},12,t.row_focus,3,t.accent); line(choices[i],510+float(i)*75); }
        } else if(status.stage==SaveSyncStage::failed) {
            line("Save synchronization failed",410); line(scene_sync_choice==0?"Retry":"Play without synchronizing",510);
        } else line("Synchronizing saves",430);
    }
    constexpr const char* faces[]{"Cross","Circle","Square","Triangle"};
    line(std::string(faces[cached_preferences.menu_bind[0]])+": confirm    "+faces[cached_preferences.menu_bind[1]]+": back / cancel",850,24);
}
bool call_scene(Pack& p,const char* method,const Json& argument) {
    try {
        const std::string name=method;
        const auto query=[&](const std::string& q) -> Json { return query_cached(p,q,Json::object()); };
        const auto sound=[&](const std::string& cue,bool music) { scene_sound(p,cue,music); };
        if(name=="input") {
            const auto physical=argument.at("key").get<std::string>();
            constexpr const char* keys[]{"cross","circle","square","triangle","options","l1","r1","up","down","left","right"};
            const auto found=std::find(std::begin(keys),std::end(keys),physical);
            scene::require(found!=std::end(keys),"unknown input");
            const auto mapped=menu_key_for(static_cast<Key>(found-std::begin(keys)),cached_preferences.menu_bind);
            constexpr const char* semantic[]{"select","back","secondary","settings","options","l1","r1","up","down","left","right"};
            const std::string key=semantic[static_cast<int>(mapped)];
            if(!scene_dialog_input(p,key)) p.scene_player->press(key,query,[&](const auto& op,const auto& args){scene_dispatch(p,op,args);},sound);
        } else if(name=="update") {
            const float dt=argument.value("dt",0.0f); p.scene_player->update(dt,cached_preferences.reduce_motion);
            if(!p.scene_music.empty()) {
                p.scene_music_left-=dt;
                if(p.scene_music_left<=0) { const auto cue=p.scene_music; p.scene_music.clear(); scene_sound(p,cue,true); }
            }
        } else if(name=="draw") {
            p.scene_player->update(0,cached_preferences.reduce_motion);
            p.scene_player->draw(query,[&](const auto& kind,const auto& args){scene_emit(p,kind,args);},sound);
        }
        return true;
    } catch(const std::exception& e) {
        if(std::string_view(method)=="input") { scene_notice=e.what(); return true; }
        last_error=e.what(); return false;
    }
}
