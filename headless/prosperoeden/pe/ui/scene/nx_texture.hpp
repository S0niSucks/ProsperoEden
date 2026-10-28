// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "pe/ui/scene/nx_image.hpp"
namespace pe::ui::scene::nx {
inline std::size_t pointer64(const Binary& b,std::size_t at) {
    require(b.u32(at+4)==0,"BNTX pointer exceeds bounded file address space");return b.u32(at);
}
// Base mip only. Tegra block-linear addresses are computed in bytes, not host GPU tiles.
inline gfx::Image bntx_texture(std::string_view source,const std::string& wanted) {
    Binary b{source};require(b.view(0,8)==std::string_view("BNTX\0\0\0\0",8)&&b.u16(12)==0xfeff,"unsupported BNTX header/byte order");
    require(b.u32(28)==source.size()&&b.view(32,4)=="NX  ","invalid BNTX extent/target");
    const auto count=b.u32(36);require(count<=128,"BNTX texture count limit");const auto table=pointer64(b,40);b.view(table,count*8);gfx::Image result;
    for(unsigned i=0;i<count;++i) {
        const auto at=pointer64(b,table+i*8);require(b.view(at,4)=="BRTI","invalid BNTX texture record");b.view(at,120);
        const auto name_at=pointer64(b,at+96);const auto name_size=b.u16(name_at);require(name_size>0&&name_size<=240,"BNTX texture name limit");
        const auto name=std::string(b.view(name_at+2,name_size));require(name.find('\0')==name.npos,"invalid BNTX texture name");if(name!=wanted)continue;
        require(result.empty(),"ambiguous BNTX texture name");const auto width=b.u32(at+36),height=b.u32(at+40),format=b.u32(at+28),log=b.u32(at+52);const auto tile=b.u16(at+18);
        require(b.u8(at+17)==2&&b.u32(at+44)==1&&b.u32(at+48)==1&&b.u16(at+24)<=1,"BNTX requires a single 2D non-multisample image");
        require(width&&height&&width<=4096&&height<=4096&&std::uint64_t(width)*height<=4194304,"BNTX image budget exceeded");
        require(tile<=1&&log<=5&&b.u16(at+22)>=1,"unsupported BNTX tile/mipmap configuration");
        // Component selectors 2/3/4/5 mean R/G/B/A. Other swizzles need an explicit decoder.
        require(b.u32(at+88)==0x05040302,"unsupported BNTX component swizzle");
        const auto codec=format>>8;require((format&255)==1,"unsupported BNTX channel type; sRGB material conversion requires color-managed baking");
        require(codec==0x0b||codec==0x1a||codec==0x1b||codec==0x1c,"unsupported BNTX format (RGBA8/BC1/BC2/BC3 only)");
        const unsigned unit=codec==0x0b?4:codec==0x1a?8:16,bw=codec==0x0b?width:(width+3)/4,bh=codec==0x0b?height:(height+3)/4;
        const auto pointers=pointer64(b,at+112),data=pointer64(b,pointers);const auto length=b.u32(at+80);const auto pixels=b.view(data,length);
        unsigned block_height=1u<<log;while(block_height>1&&bh<=8*(block_height/2))block_height/=2;
        const std::size_t width_gobs=(bw*unit+63)/64,pitch=(bw*unit+31)&~std::size_t(31);std::string linear(std::size_t(bw)*bh*unit,'\0');
        for(unsigned y=0;y<bh;++y)for(unsigned x=0;x<bw;++x) {
            const std::size_t xb=std::size_t(x)*unit;
            const auto address=tile==1?y*pitch+xb:(y/(8*block_height))*512*block_height*width_gobs+(xb/64)*512*block_height+((y%(8*block_height))/8)*512+(xb%64/32)*256+(y%8/2)*64+(xb%32/16)*32+(y%2)*16+xb%16;
            require(address<=pixels.size()&&unit<=pixels.size()-address,"BNTX tiled data is truncated");std::memcpy(linear.data()+(std::size_t(y)*bw+x)*unit,pixels.data()+address,unit);
        }
        if(codec==0x0b){result.width=width;result.height=height;result.rgba.assign(linear.begin(),linear.end());}
        else {
            std::string dds(128,'\0');std::memcpy(dds.data(),"DDS ",4);
            const auto word=[&](unsigned p,unsigned v){for(unsigned k=0;k<4;++k)dds[p+k]=char(v>>(8*k));};word(4,124);word(12,height);word(16,width);word(76,32);word(80,4);
            std::memcpy(dds.data()+84,codec==0x1a?"DXT1":codec==0x1b?"DXT3":"DXT5",4);result=image(dds+linear);
        }
    }return result;
}
inline gfx::Image firmware_texture(const Files& files,const std::string& name) {
    gfx::Image result;for(const auto& [path,bytes]:files)if(path.ends_with(".bntx")) {
        auto decoded=bntx_texture(bytes,name);if(decoded.empty())continue;require(result.empty(),"texture name occurs in several BNTX archives: "+name);result=std::move(decoded);
    }
    require(!result.empty(),"firmware texture missing: "+name);return result;
}
} // namespace pe::ui::scene::nx
