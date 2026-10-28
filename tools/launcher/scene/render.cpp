// SPDX-License-Identifier: GPL-3.0-or-later
// Focused scene-to-existing-OpenGL-backend test. This is not a launcher screenshot/PS5 qualification.
#include "pe/ui/scene/player.hpp"
#include "pe/gfx/gl_batch.hpp"
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <cassert>
#include <iostream>

using namespace pe::ui::scene;
int main() {
    const auto get_display = reinterpret_cast<PFNEGLGETPLATFORMDISPLAYEXTPROC>(eglGetProcAddress("eglGetPlatformDisplayEXT"));
    assert(get_display);
    const auto display = get_display(EGL_PLATFORM_SURFACELESS_MESA,EGL_DEFAULT_DISPLAY,nullptr);
    EGLint major{}, minor{}; assert(eglInitialize(display,&major,&minor)); assert(eglBindAPI(EGL_OPENGL_API));
    const EGLint attributes[]{EGL_CONTEXT_MAJOR_VERSION,4,EGL_CONTEXT_MINOR_VERSION,5,EGL_CONTEXT_OPENGL_PROFILE_MASK,EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT,EGL_NONE};
    const auto context = eglCreateContext(display,EGL_NO_CONFIG_KHR,EGL_NO_CONTEXT,attributes);
    assert(context != EGL_NO_CONTEXT && eglMakeCurrent(display,EGL_NO_SURFACE,EGL_NO_SURFACE,context));
    {
        pe::gfx::GlBatch batch; assert(batch.init());
        const unsigned char texels[]{255,0,0,255, 0,255,0,255, 0,0,255,255, 255,255,255,255};
        const auto image = batch.create_texture(2,2,texels); assert(image);
        auto description = Json::parse(R"({"schema":1,"id":"render-test","entry":"home","screens":{"home":"screen.json"},"images":{"image":"image.tga"}})");
        auto screen = Json::parse(R"({"transition":0,"nodes":[
            {"id":"background","type":"rect","w":1920,"h":1080,"color":0},
            {"id":"clipped","type":"mask","x":100,"y":100,"w":200,"h":200,"children":[{"id":"red","type":"rect","w":500,"h":500,"color":16711680}]},
            {"id":"atlas","type":"image","asset":"image","x":700,"y":100,"w":300,"h":300,"uv":[0,0,0.49,0.49]},
            {"id":"panel","type":"nineslice","asset":"image","x":1200,"y":100,"w":400,"h":400,"slice":[0.2,0.2,0.2,0.2],"border":24},
            {"id":"moving","type":"rect","x":100,"y":700,"w":200,"h":150,"color":65280},
            {"id":"dim","type":"rect","x":700,"y":700,"w":100,"h":100,"color":16711680,"alpha":0.5},
            {"id":"border","type":"rect","x":1000,"y":700,"w":200,"h":150,"color":0,"edge":16777215,"border":16}
        ],"animations":["motion"]})");
        auto tracks = Json::parse(R"({"motion":[{"node":"moving","property":"x","duration":1,"keys":[{"at":0,"value":100},{"at":1,"value":400}]}]})");
        Player player(parse(description,{{"home",screen}},tracks)); player.update(0,true);
        for (const auto& [width,height] : {std::pair{1280,720},std::pair{1920,1080}}) {
            GLuint color{}, framebuffer{}; glGenTextures(1,&color); glBindTexture(GL_TEXTURE_2D,color);
            glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,width,height,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);
            glGenFramebuffers(1,&framebuffer); glBindFramebuffer(GL_FRAMEBUFFER,framebuffer);
            glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,color,0);
            assert(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE);
            pe::gfx::DrawList list;
            player.draw([](const auto&){return Json::object();},[&](const std::string& kind,const Json& p) {
                const pe::gfx::Rect r{p.value("x",0.0f),p.value("y",0.0f),p.value("w",0.0f),p.value("h",0.0f)};
                if(kind=="rect") list.rounded_rect(r,p.value("radius",0.0f),pe::gfx::Color::rgb(p.value("color",0xffffff),p.value("alpha",1.0f)));
                else if(kind=="border") list.bordered_rect(r,p.value("radius",0.0f),pe::gfx::Color::rgb(p.value("color",0xffffff)),p.at("border"),pe::gfx::Color::rgb(p.at("edge").get<int>()));
                else if(kind=="image") { const auto uv=p.value("uv",std::vector<float>{0,0,1,1}); list.image(image,r,{uv[0],uv[1],uv[2],uv[3]},pe::gfx::Color::rgb(0xffffff,p.value("alpha",1.0f))); }
                else if(kind=="clip") list.push_clip(r);
                else if(kind=="unclip") list.pop_clip();
                else if(kind=="transform") list.push_transform(p.value("scale",1.0f),p.value("ox",0.0f),p.value("oy",0.0f),p.value("dx",0.0f),p.value("dy",0.0f));
                else if(kind=="untransform") list.pop_transform();
                else if(kind=="opacity") list.push_opacity(p.value("value",1.0f));
                else if(kind=="unopacity") list.pop_opacity();
                else assert(false);
            },[](const auto&,bool){});
            glViewport(0,0,width,height); glClearColor(0,0,0,1); glClear(GL_COLOR_BUFFER_BIT);
            batch.draw(list,pe::gfx::fit_viewport(width,height),width,height); glFinish(); assert(glGetError()==GL_NO_ERROR);
            const auto pixel=[&](float x,float y) {
                unsigned char rgba[4]{}; glReadPixels(int(x*width/1920.0f),height-1-int(y*height/1080.0f),1,1,GL_RGBA,GL_UNSIGNED_BYTE,rgba);
                return std::array<int,3>{rgba[0],rgba[1],rgba[2]};
            };
            assert((pixel(150,150)==std::array<int,3>{255,0,0}));
            assert((pixel(350,150)==std::array<int,3>{0,0,0})); // mask did not leak
            assert(pixel(750,150)[0]>240 && pixel(750,150)[1]<15); // UV region
            assert((pixel(450,750)==std::array<int,3>{0,255,0})); // reduced motion settled x=400
            assert((pixel(150,750)==std::array<int,3>{0,0,0}));
            assert(pixel(750,750)[0]>=127 && pixel(750,750)[0]<=128); // subtree alpha is applied once
            assert((pixel(1010,750)==std::array<int,3>{255,255,255}));
            assert((pixel(1100,750)==std::array<int,3>{0,0,0}));
            assert(list.instances().size()==15); // base + mask child + image + 9 slices + moving + alpha + border
            std::cout<<"scene GL "<<width<<'x'<<height<<": "<<list.instances().size()<<" instances, "<<batch.last_draw_calls()<<" batches, pixels passed\n";
            glDeleteFramebuffers(1,&framebuffer); glDeleteTextures(1,&color);
        }
        batch.delete_texture(image);
    }
    eglMakeCurrent(display,EGL_NO_SURFACE,EGL_NO_SURFACE,EGL_NO_CONTEXT); eglDestroyContext(display,context); eglTerminate(display);
}
