// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "pe/ui/scene/nx_layout.hpp"
#include "pe/ui/scene/nx_texture.hpp"
namespace pe::ui::scene::nx {
// Bake the explicitly supported single-sampler, axis-aligned pipeline into bounded RGBA.
// Original material records remain in the report; unknown combiners are never approximated.
inline gfx::Image bake_material(const Layout& layout,const Pane& pane,const Files& firmware) {
    require(pane.material<layout.materials.size(),"picture material index outside table");const auto& m=layout.materials[pane.material];
    require((m.flags&~0x3fu)==0&&m.references.size()<=1&&m.transforms.size()<=1,"unsupported material combiner/coordinate pipeline");
    require(pane.uniform_color,"nonuniform four-corner vertex color is not supported");
    require(pane.w>0&&pane.h>0&&pane.w<=2048&&pane.h<=2048,"material bake dimensions exceed budget");
    gfx::Image source;unsigned wrap_s=0,wrap_t=0;bool linear=false;float tx=0,ty=0,sx=1,sy=1;
    if(!m.references.empty()) {
        require(pane.uvsets==1&&((m.flags>>4)&3)<=1,"material requires exactly one supported UV set");
        require(std::abs(pane.uv[0]+pane.uv[6]-pane.uv[2]-pane.uv[4])<0.0001f&&std::abs(pane.uv[1]+pane.uv[7]-pane.uv[3]-pane.uv[5])<0.0001f,"non-affine picture UVs require original triangle interpolation");
        const auto& ref=m.references[0];source=firmware_texture(firmware,ref.at("Name"));const auto ws=ref.at("WrapS").get<unsigned>(),wt=ref.at("WrapT").get<unsigned>();
        require(ws<=6&&wt<=6&&(ws&3)<=2&&(wt&3)<=2&&((ws^wt)&4)==0,"unsupported material wrap/filter combination");wrap_s=ws&3;wrap_t=wt&3;linear=ws&4;
        if(!m.transforms.empty()){const auto& transform=m.transforms[0];require(transform.at("Rotation").get<float>()==0,"rotated texture transforms are not supported");tx=transform.at("X");ty=transform.at("Y");sx=transform.at("ScaleX");sy=transform.at("ScaleY");}
    }
    gfx::Image out;out.width=std::max(1,int(std::ceil(pane.w)));out.height=std::max(1,int(std::ceil(pane.h)));out.rgba.resize(std::size_t(out.width)*out.height*4);
    const auto coordinate=[](std::int64_t p,int n,unsigned wrap){if(wrap==0)return int(std::clamp(p,std::int64_t(0),std::int64_t(n-1)));const int period=wrap==1?n:2*n;int value=p%period;if(value<0)value+=period;return wrap==2&&value>=n?2*n-1-value:value;};
    const auto sample=[&](float u,float v,unsigned channel){
        const double x=double(u)*source.width-0.5,y=double(v)*source.height-0.5;const auto ix=std::int64_t(std::floor(x)),iy=std::int64_t(std::floor(y));
        const auto texel=[&](std::int64_t px,std::int64_t py){return float(source.rgba[(std::size_t(coordinate(py,source.height,wrap_t))*source.width+coordinate(px,source.width,wrap_s))*4+channel]);};
        if(!linear)return texel(std::int64_t(std::floor(double(u)*source.width)),std::int64_t(std::floor(double(v)*source.height)));
        const float fx=x-ix,fy=y-iy;return (texel(ix,iy)*(1-fx)+texel(ix+1,iy)*fx)*(1-fy)+(texel(ix,iy+1)*(1-fx)+texel(ix+1,iy+1)*fx)*fy;
    };
    for(int y=0;y<out.height;++y)for(int x=0;x<out.width;++x){const float fx=(x+0.5f)/out.width,fy=(y+0.5f)/out.height;
        const float u=((pane.uv[0]*(1-fx)+pane.uv[2]*fx)*(1-fy)+(pane.uv[4]*(1-fx)+pane.uv[6]*fx)*fy-0.5f)*sx+0.5f+tx;
        const float v=((pane.uv[1]*(1-fx)+pane.uv[3]*fx)*(1-fy)+(pane.uv[5]*(1-fx)+pane.uv[7]*fx)*fy-0.5f)*sy+0.5f+ty;
        require(std::isfinite(u)&&std::isfinite(v)&&std::abs(u)<1000000&&std::abs(v)<1000000,"material UV range exceeded");
        for(unsigned c=0;c<4;++c){const unsigned shift=24-c*8;const float fore=(m.foreground>>shift)&255,back=(m.background>>shift)&255,vertex=(pane.rgba>>shift)&255;
            const float value=source.empty()?fore:back+(fore-back)*sample(u,v,c)/255;out.rgba[(std::size_t(y)*out.width+x)*4+c]=std::uint8_t(std::clamp(std::lround(value*vertex/255),0l,255l));}
    }
    return out;
}
} // namespace pe::ui::scene::nx
