// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "pe/ui/scene/nx_archive.hpp"
// The already-vendored decoder stays TU-local; console imports never invoke author tooling.
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#endif
#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#define STBI_MAX_DIMENSIONS 4096
#include "stb/stb_image.h"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
namespace pe::ui::scene::nx {
inline gfx::Image image(std::string_view source) {
    Binary b{source};gfx::Image out;
    if(source.starts_with("DDS ")) {
        require(b.u32(4)==124 && b.u32(76)==32 && (b.u32(80)&4) && b.u32(112)==0,"unsupported DDS header");
        out.width=b.u32(16);out.height=b.u32(12);const auto kind=b.view(84,4);
        require(kind=="DXT1"||kind=="DXT3"||kind=="DXT5","unsupported DDS codec; expected DXT1/3/5");
        require(out.width>0 && out.height>0 && out.width<=4096 && out.height<=4096 && std::uint64_t(out.width)*out.height<=4194304,"DDS dimensions limit");
        const int block=kind=="DXT1"?8:16;b.view(128,((out.width+3)/4)*((out.height+3)/4)*block);out.rgba.resize(std::size_t(out.width)*out.height*4);std::size_t at=128;
        const auto rgb=[](unsigned v){return std::array<unsigned,4>{((v>>11)&31)*255/31,((v>>5)&63)*255/63,(v&31)*255/31,255};};
        for(int y=0;y<out.height;y+=4)for(int x=0;x<out.width;x+=4,at+=block) {
            const auto c=at+block-8;const auto a=b.u16(c),z=b.u16(c+2),bits=b.u32(c+4);std::array<std::array<unsigned,4>,4> colors{rgb(a),rgb(z),{}, {}};
            for(int k=0;k<3;++k){colors[2][k]=(a>z||block==16)?(2*colors[0][k]+colors[1][k])/3:(colors[0][k]+colors[1][k])/2;colors[3][k]=(a>z||block==16)?(colors[0][k]+2*colors[1][k])/3:0;}
            colors[2][3]=255;colors[3][3]=(a>z||block==16)?255:0;
            std::array<unsigned,8> alpha{};std::uint64_t abits=0;
            if(kind=="DXT5") {alpha[0]=b.u8(at);alpha[1]=b.u8(at+1);for(int i=0;i<6;++i)abits|=std::uint64_t(b.u8(at+2+i))<<(8*i);
                if(alpha[0]>alpha[1])for(unsigned i=1;i<=6;++i)alpha[i+1]=((7-i)*alpha[0]+i*alpha[1])/7;
                else {for(unsigned i=1;i<=4;++i)alpha[i+1]=((5-i)*alpha[0]+i*alpha[1])/5;alpha[6]=0;alpha[7]=255;}}
            for(int py=0;py<4;++py)for(int px=0;px<4;++px)if(x+px<out.width && y+py<out.height) {
                const auto n=py*4+px;auto rgba=colors[(bits>>(2*n))&3];
                if(kind=="DXT3")rgba[3]=((b.u8(at+n/2)>>(4*(n%2)))&15)*17;
                if(kind=="DXT5")rgba[3]=alpha[(abits>>(3*n))&7];
                for(int k=0;k<4;++k)out.rgba[(std::size_t(y+py)*out.width+x+px)*4+k]=rgba[k];
            }
        }
    } else {
        require(source.starts_with("\x89PNG")||source.starts_with("\xff\xd8\xff"),"unsupported image; expected DDS/PNG/JPEG");int channels{};
        const auto* bytes=reinterpret_cast<const unsigned char*>(source.data());const int size=source.size();
        require(stbi_info_from_memory(bytes,size,&out.width,&out.height,&channels) && out.width>0 && out.height>0 && out.width<=4096 && out.height<=4096 && std::uint64_t(out.width)*out.height<=4194304,"image dimensions limit");
        auto* pixels=stbi_load_from_memory(bytes,size,&out.width,&out.height,&channels,4);require(pixels!=nullptr,"invalid PNG/JPEG");
        out.rgba.assign(pixels,pixels+std::size_t(out.width)*out.height*4);stbi_image_free(pixels);
    }return out;
}
inline std::string encode_tga(const gfx::Image& image) {
    std::string bytes(18+image.rgba.size(),'\0');bytes[2]=2;bytes[12]=char(image.width);bytes[13]=char(image.width>>8);bytes[14]=char(image.height);bytes[15]=char(image.height>>8);bytes[16]=32;bytes[17]=40;
    for(std::size_t i=0;i<image.rgba.size();i+=4){bytes[18+i]=char(image.rgba[i+2]);bytes[19+i]=char(image.rgba[i+1]);bytes[20+i]=char(image.rgba[i]);bytes[21+i]=char(image.rgba[i+3]);}return bytes;
}
} // namespace pe::ui::scene::nx
