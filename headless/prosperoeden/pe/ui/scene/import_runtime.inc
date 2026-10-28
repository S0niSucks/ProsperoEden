// SPDX-License-Identifier: GPL-3.0-or-later
// Host-owned importer; package data can request this screen, never drive/confirm it.
bool import_page{},import_partial{};
int import_row{},import_report_row{};
std::string import_phase="files",import_error;
std::vector<std::string> import_sources;
std::unique_ptr<scene::nx::Conversion> import_plan;
std::future<scene::nx::Conversion> import_read;
std::future<std::string> import_install;
std::uint32_t import_texture{};
ThemeImportStorage import_storage;
void release_import_texture(){batch.delete_texture(import_texture);import_texture=0;}
void open_import() {
    if(!update_available() || pack_updates->busy() || updates_page || commit_started) {last_error="Finish the pending operation before importing a theme";return;}
    import_error.clear();import_page=true;picker=false;import_row=import_report_row=0;
    if(import_read.valid()||import_install.valid())return;
    import_plan.reset();release_import_texture();import_phase="files";
    if(mixer)mixer->stop_all();
    try {
        runtime_fs::Descriptor root(data_root,runtime_fs::Kind::directory);scene::nx::Fd imports(scene::nx::child_dir(root.get(),"theme-import"));
        scene::nx::Fd inbox(scene::nx::child_dir(imports.value,"inbox"));scene::nx::Fd firmware(scene::nx::child_dir(imports.value,"firmware"));
        import_sources.clear();for(const auto& name:runtime_fs::entries(data_root+"/theme-import/inbox")) {
            const auto lower=scene::nx::folded(name);if(lower.ends_with(".nxtheme")||lower.ends_with(".zip"))import_sources.push_back(name);
            scene::require(import_sources.size()<=64,"too many import sources");
        }
    } catch(const std::exception& e){import_error=e.what();}
}
void poll_import() {
    if(import_read.valid() && import_read.wait_for(std::chrono::seconds(0))==std::future_status::ready) {
        try{import_plan=std::make_unique<scene::nx::Conversion>(import_read.get());import_phase="preview";}
        catch(const std::exception& e){import_error=e.what();import_phase="failed";}
    }
    if(import_install.valid() && import_install.wait_for(std::chrono::seconds(0))==std::future_status::ready) {
        try{const auto id=import_install.get();import_phase="installed";discover();std::fprintf(stderr,"PE_THEME_IMPORTED id=%s\n",id.c_str());}
        catch(const std::exception& e){import_error=e.what();import_phase="failed";}
    }
}
void press_import(Key physical) {
    const auto key=menu_key_for(physical,cached_preferences.menu_bind);
    if(key==Key::options && import_phase!="installing") {import_page=false;discover();picker=true;picker_row=0;return;}
    if(key==Key::circle) {
        if(import_phase=="installing")return;
        if(import_phase=="consent"){import_phase="preview";return;}
        import_page=false;release_import_texture();return;
    }
    if(import_phase=="files") {
        if(key==Key::up||key==Key::down)import_row=std::clamp(import_row+(key==Key::down?1:-1),0,std::max(0,int(import_sources.size())-1));
        if(key==Key::cross&&!import_sources.empty()) {
            import_phase="reading";import_error.clear();const auto root=data_root,name=import_sources.at(import_row);
            import_read=std::async(std::launch::async,[root,name]{scene::Reader reader(root+"/theme-import/inbox");const auto bytes=reader.read(name,scene::nx::archive_limit);
                return scene::nx::convert(bytes,[root](const std::string& target){scene::Reader firmware(root+"/theme-import/firmware");return firmware.read(target+".szs",scene::nx::archive_limit);});});
        }
    } else if(import_phase=="preview"&&import_plan) {
        if(key==Key::up||key==Key::down)import_report_row=std::clamp(import_report_row+(key==Key::down?1:-1),0,std::max(0,int(import_plan->report["omissions"].size())-1));
        if(key==Key::cross && import_plan->report["omissions"].empty()){import_partial=false;import_phase="consent";}
        else if(key==Key::square){import_partial=true;import_phase="consent";}
        else if(key==Key::cross)import_error="Unsupported items remain. Review the report; secondary explicitly accepts a partial import.";
    } else if(import_phase=="consent" && key==Key::cross && import_plan) {
        import_phase="installing";import_error.clear();const auto root=data_root;const auto plan=*import_plan;const auto partial=import_partial;const auto storage=import_storage;
        import_install=std::async(std::launch::async,[root,plan,partial,storage]{return scene::nx::install(root,plan,true,partial,storage);});
    } else if(import_phase=="installed" && key==Key::cross && import_plan){const auto id=import_plan->id;import_page=false;release_import_texture();select(id);}
    else if(import_phase=="failed" && key==Key::cross)open_import();
}
void draw_import(gfx::DrawList& list) {
    list.reset_state();const auto& t=th();list.rounded_rect({0,0,1920,1080},0,t.base);
    auto line=[&](const std::string& text,float x,float y,float width,float size=28){list.text(*fonts.font,fonts.texture,fonts.font->fit(tr(text.c_str()),text_size(size),width),x,y,text_size(size),t.text);};
    line("Import an offline NXTheme",160,120,1600,44);
    if(import_phase=="files") {
        line("Copy your .nxtheme or theme-pack ZIP to:",160,210,1600);line(data_root+"/theme-import/inbox",160,260,1600,24);
        line("Optional user-extracted firmware: theme-import/firmware/<target>.szs",160,310,1600,24);
        if(import_sources.empty())line("No packages found. Back exits; reopen after copying your files.",160,450,1600);
        for(int i=0;i<int(import_sources.size());++i)if(i>=import_row/6*6&&i<import_row/6*6+6) {
            const float y=370+float(i%6)*75;if(i==import_row)list.bordered_rect({150,y,1620,65},12,t.row_focus,3,t.accent);
            line(import_sources[i],175,y+45,1550);
        }
    } else if(import_phase=="reading"||import_phase=="installing")line(import_phase=="reading"?"Reading bounded package and building import report…":"Installing validated staged package. Do not power off…",160,300,1600);
    else if(import_plan) {
        line(import_plan->name,160,220,750,36);line(import_plan->id,160,275,750,24);
        line("Artwork preview — not faithful parity",990,220,770,24);
        if(!import_texture&&!import_plan->preview.empty())import_texture=batch.create_texture(import_plan->preview.width,import_plan->preview.height,import_plan->preview.rgba.data());
        if(import_texture)list.image(import_texture,{990,250,768,432},{0,0,1,1},{1,1,1,1});
        const auto& report=import_plan->report;line("Source SHA-256:",160,340,750,24);line(report.value("source_sha256",""),160,385,750,20);
        line("Unsupported / remapped items: "+std::to_string(report.at("omissions").size()),160,460,750);
        line("No Nintendo assets or keys are supplied.",160,520,750,24);
        line("Converted data stays in this console's launcher folder.",160,565,750,24);
        if(import_phase=="preview") {
            if(!report.at("omissions").empty()) {const auto& row=report.at("omissions").at(import_report_row);line(row.value("path","")+": "+row.value("code",""),160,750,1600,26);line(row.value("detail",""),160,800,1600,24);}
            line("Up / Down: report · Confirm: continue without omissions · Secondary: explicitly accept partial import",160,875,1600,24);
        } else if(import_phase=="consent") {
            line(import_partial?"Install PARTIAL conversion with the listed omissions?":"Install this converted artwork and original native workflows?",160,760,1600,30);
            line("Confirm installs a NEW ID only. Existing launchers and your selection are never overwritten.",160,825,1600,24);
        } else if(import_phase=="installed") {line("Installed. Confirm selects this launcher; Back keeps your current launcher.",160,780,1600,28);}
    }
    if(!import_error.empty())line(import_error,160,930,1600,24);
    line("Back: cancel / close · Options: built-in recovery picker",160,1010,1600,24);
}
