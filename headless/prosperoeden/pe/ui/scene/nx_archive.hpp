// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "pe/ui/scene/package.hpp"
#include <miniz.h>
#include <openssl/sha.h>
#include <array>
#include <cctype>
#include <string_view>
#include <functional>

namespace pe::ui::scene::nx {
constexpr std::size_t archive_limit=64*1024*1024, member_limit=16*1024*1024;
using Files=std::map<std::string,std::string>;
struct Binary {
    std::string_view bytes; bool big{};
    std::string_view view(std::size_t at,std::size_t n) const {require(at<=bytes.size() && n<=bytes.size()-at,"truncated binary at "+std::to_string(at));return bytes.substr(at,n);}
    unsigned u8(std::size_t at) const {return static_cast<unsigned char>(view(at,1)[0]);}
    std::uint32_t u16(std::size_t at) const {return big?(u8(at)<<8)|u8(at+1):u8(at)|(u8(at+1)<<8);}
    std::uint32_t u32(std::size_t at) const {return big?(u16(at)<<16)|u16(at+2):u16(at)|(u16(at+2)<<16);}
    float f32(std::size_t at) const {const auto word=u32(at);float f;std::memcpy(&f,&word,4);require(std::isfinite(f) && std::abs(f)<=1000000,"invalid binary float");return f;}
    std::string name(std::size_t at,std::size_t max) const {const auto v=view(at,max);const auto end=v.find('\0');require(end!=v.npos,"unterminated binary name");return std::string(v.substr(0,end));}
};
inline std::string sha256(std::string_view bytes) {
    unsigned char hash[SHA256_DIGEST_LENGTH];SHA256(reinterpret_cast<const unsigned char*>(bytes.data()),bytes.size(),hash);
    std::string out;for(auto b:hash){out+="0123456789abcdef"[b>>4];out+="0123456789abcdef"[b&15];}return out;
}
inline Json json(std::string_view bytes) {
    require(bytes.size()<=512*1024,"NX JSON exceeds limit");int depth=0;bool quoted=false,escaped=false;
    for(char c:bytes) {if(quoted){if(escaped)escaped=false;else if(c=='\\')escaped=true;else if(c=='"')quoted=false;}
        else if(c=='"')quoted=true;else if(c=='{'||c=='[')require(++depth<=32,"NX JSON depth exceeded");else if(c=='}'||c==']')--depth;}
    std::vector<std::set<std::string>> keys;
    return Json::parse(bytes,[&](int,Json::parse_event_t event,Json& value){
        if(event==Json::parse_event_t::object_start)keys.emplace_back();
        if(event==Json::parse_event_t::key)require(keys.back().insert(value.get<std::string>()).second,"duplicate NX JSON member");
        if(event==Json::parse_event_t::object_end)keys.pop_back();
        return true;});
}
inline std::string yaz0(std::string_view source) {
    Binary b{source,true};require(b.view(0,4)=="Yaz0","expected Yaz0");const auto size=b.u32(4);b.view(0,16);
    require(size>0 && size<=archive_limit,"Yaz0 expansion exceeds limit");std::string out;out.reserve(size);std::size_t at=16;
    while(out.size()<size) {const auto flags=b.u8(at++);for(int bit=7;bit>=0 && out.size()<size;--bit) {
        if(flags&(1<<bit))out.push_back(static_cast<char>(b.u8(at++)));
        else {const auto a=b.u8(at++),c=b.u8(at++);const auto distance=((a&15)<<8|c)+1;auto n=a>>4;n=n?n+2:b.u8(at++)+18;
            require(distance<=out.size() && n<=size-out.size(),"invalid Yaz0 back-reference");while(n--)out.push_back(out[out.size()-distance]);}
    }}return out;
}
inline std::string folded(std::string name) {for(auto& c:name)if(c>='A'&&c<='Z')c=char(c-'A'+'a');return name;}
inline void member_name(const std::string& name,std::set<std::string>& names) {
    require(runtime_fs::relative_path(name) && std::count(name.begin(),name.end(),'/')<=12,"unsafe NX archive name: "+name);
    require(names.insert(folded(name)).second,"duplicate/case-colliding NX archive member");
}
inline Files sarc(std::string_view source) {
    Binary b{source};require(b.view(0,4)=="SARC","expected SARC");const auto bom=b.view(6,2);
    require(bom==std::string_view("\xfe\xff",2)||bom==std::string_view("\xff\xfe",2),"invalid SARC endian marker");b.big=bom==std::string_view("\xfe\xff",2);
    require(b.u16(4)==20 && b.u32(8)==source.size() && b.u16(16)==0x100,"invalid SARC header");
    const auto body=b.u32(12);require(body<=source.size() && b.view(20,4)=="SFAT" && b.u16(24)==12,"invalid SFAT");
    const auto count=b.u16(26),multiplier=b.u32(28);require(count>0 && count<=256,"SARC count limit");const auto sfnt=32+count*16;
    require(b.view(sfnt,4)=="SFNT" && b.u16(sfnt+4)==8 && sfnt+8<=body,"invalid SFNT");
    Files files;std::set<std::string> names;std::vector<std::pair<unsigned,unsigned>> ranges;
    for(unsigned i=0;i<count;++i) {const auto at=32+i*16,attr=b.u32(at+4),begin=b.u32(at+8),end=b.u32(at+12);
        require((attr>>24)==1 && begin<=end && end<=source.size()-body && end-begin<=member_limit,"invalid SARC entry");
        const std::uint64_t pos=std::uint64_t(sfnt)+8+std::uint64_t(attr&0xffffff)*4;require(pos<body,"SARC name outside table");
        const auto name=b.name(pos,std::min<std::size_t>(241,body-pos));member_name(name,names);std::uint32_t hash=0;for(unsigned char c:name)hash=hash*multiplier+c;
        require(hash==b.u32(at),"SARC name hash mismatch");for(auto [a,z]:ranges)require(end<=a||begin>=z,"overlapping SARC data");ranges.emplace_back(begin,end);
        files.emplace(name,b.view(body+begin,end-begin));
    }return files;
}
inline Files zip(std::string_view source) {
    struct Archive {mz_zip_archive value{};~Archive(){mz_zip_reader_end(&value);}} archive;auto& z=archive.value;
    require(mz_zip_reader_init_mem(&z,source.data(),source.size(),0),"invalid NX ZIP");const auto count=mz_zip_reader_get_num_files(&z);
    require(count>0 && count<=256,"ZIP count limit");Files files;std::set<std::string> names;std::uint64_t total=0;
    for(mz_uint i=0;i<count;++i) {mz_zip_archive_file_stat s{};require(mz_zip_reader_file_stat(&z,i,&s),"invalid ZIP entry");std::string name=s.m_filename;
        require(mz_zip_reader_get_filename(&z,i,nullptr,0)==name.size()+1,"invalid ZIP filename extent");const bool dir=mz_zip_reader_is_file_a_directory(&z,i);
        if(dir && !name.empty() && name.back()=='/')name.pop_back();
        member_name(name,names);
        const auto mode=(s.m_external_attr>>16)&S_IFMT;require(mode==0||mode==(dir?S_IFDIR:S_IFREG),"ZIP links/special files forbidden");
        require(!s.m_is_encrypted && s.m_is_supported && (s.m_method==0||s.m_method==8),"unsupported ZIP compression/encryption");
        require(s.m_uncomp_size<=member_limit && total+s.m_uncomp_size<=archive_limit,"ZIP expansion limit");total+=s.m_uncomp_size;
        if(!dir){std::string bytes(s.m_uncomp_size,'\0');require(mz_zip_reader_extract_to_mem(&z,i,bytes.data(),bytes.size(),0),"ZIP CRC/extraction failure");files.emplace(name,std::move(bytes));}
    }return files;
}
inline Files container(std::string_view bytes) {
    require(!bytes.empty() && bytes.size()<=archive_limit,"NX source size limit");
    if(bytes.starts_with("Yaz0"))return sarc(yaz0(bytes));
    if(bytes.starts_with("SARC"))return sarc(bytes);
    if(bytes.starts_with("PK\x03\x04"))return zip(bytes);
    throw std::runtime_error("NX: expected ZIP or Yaz0/SARC; encrypted firmware/NCA/keys are not accepted");
}
} // namespace pe::ui::scene::nx
