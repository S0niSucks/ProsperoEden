#!/usr/bin/env python3
"""C++20 ASan/UBSan check for real maintained JIT RX/RW alias lookups."""
from pathlib import Path
import shutil
import subprocess
import tempfile

root=Path(__file__).resolve().parents[1]
source=(root/"headless/jit-allocator.h").read_text()
assert "class AliasPositiveCache" in source
assert "static thread_local ::Eden::Jit::AliasPositiveCache cache{}" in source
assert "cache.Find(pointer, generation)" in source
assert "cache.Remember(pointer, writable, latest)" in source
assert "const std::size_t slot = ((bits >> 12)" not in source
memo = source
assert "std::array<Entry, 8>" in memo
assert "if (!entry.executable) return nullptr;" in memo
assert "mutable std::size_t preferred_slot{};" in memo
assert "if (i == preferred_slot) continue;" in memo
assert "if (entries[2].executable) preferred_slot = i;" in memo
assert "if (generation != current_generation) return nullptr;" in memo
# A newly inserted alias cannot invalidate any previously live RX/RW mapping.
# Only free() retires aliases and advances the TLS generation. Verify the
# actual allocator source, not merely an equivalent cache model.
alloc_body=source.split("std::uint8_t* alloc(std::size_t size) override {",1)[1].split(
    "std::uint8_t* writableAddress(",1)[0]
free_body=source.split("void free(std::uint8_t* pointer) override {",1)[1].split(
    "bool useProtect() const override",1)[0]
assert "mapping_generation.fetch_add(" not in alloc_body
assert free_body.count("mapping_generation.fetch_add(1, std::memory_order_release);")==2
assert source.count("mapping_generation.fetch_add(1, std::memory_order_release);")==2
compiler=shutil.which("clang++-18") or shutil.which("clang++")
assert compiler
stub="""
#pragma once
#include <cstdint>
#include <cstddef>
namespace Xbyak {
class Allocator {
public:
 virtual ~Allocator()=default;
 virtual std::uint8_t* alloc(std::size_t)=0;
 virtual void free(std::uint8_t*)=0;
 virtual std::uint8_t* writableAddress(std::uint8_t* p){return p;}
 virtual bool useProtect() const{return true;}
};
}
"""
fixture=r"""
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <thread>
#include <new>
#include "jit-allocator.h"
namespace Common {
void* AllocateMemoryPages(std::size_t n) noexcept {return std::malloc(n);}
void FreeMemoryPages(void* p) noexcept {std::free(p);}
void* MapExecutableAlias(void*, std::size_t) noexcept {return nullptr;}
}
int main(){
 auto* allocator=EdenJitAllocator();
 // PS5's 16 MiB-aligned executable arenas can all hash to the same
 // old single-slot memo entry: eight such keys must coexist, lock-free.
 Eden::Jit::AliasPositiveCache aliases;
 auto* unknown=reinterpret_cast<std::uint8_t*>(0x20000000ULL);
 assert(aliases.Find(unknown, 7)==nullptr); // empty cache stops immediately
 std::uint8_t* rx[8]{}; std::uint8_t* rw[8]{};
 for(unsigned i=0;i<8;++i){
   rx[i]=reinterpret_cast<std::uint8_t*>(0x10000000ULL+i*0x01000000ULL);
   rw[i]=reinterpret_cast<std::uint8_t*>(0x90000000ULL+i*0x01000000ULL);
   const auto bits=reinterpret_cast<std::uintptr_t>(rx[i]);
   assert(((bits>>12)^(bits>>21))%8==0);
   aliases.Remember(rx[i],rw[i],7);
 }
 for(unsigned i=0;i<100000;++i)
   assert(aliases.Find(rx[i%8],7)==rw[i%8]);
 // Capacity is fixed at eight: the ninth distinct arena evicts exactly
 // the oldest positive hint, not an arbitrary address-hash collision.
 auto* ninth=reinterpret_cast<std::uint8_t*>(0x18000000ULL);
 auto* ninth_rw=reinterpret_cast<std::uint8_t*>(0x98000000ULL);
 aliases.Remember(ninth,ninth_rw,7);
 assert(aliases.Find(ninth,7)==ninth_rw);
 assert(!aliases.Find(rx[0],7));
 for(unsigned i=1;i<8;++i) assert(aliases.Find(rx[i],7)==rw[i]);
 // The most recently used JIT code arena has a one-probe lookup.
 // Verify that a hot run performs NO TLS writes: protecting the fixture
 // with mprotect would crash if Find writes on same-arena hits.
 const long page=sysconf(_SC_PAGESIZE);assert(page>0);
 void* storage=mmap(nullptr,static_cast<std::size_t>(page),PROT_READ|PROT_WRITE,
                    MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
 assert(storage!=MAP_FAILED);
 auto* memo=new(storage) Eden::Jit::AliasPositiveCache{};
 for(unsigned i=0;i<8;++i) memo->Remember(rx[i],rw[i],7);
 assert(memo->Find(rx[7],7)==rw[7]);
 assert(mprotect(storage,static_cast<std::size_t>(page),PROT_READ)==0);
 for(unsigned i=0;i<10000;++i) assert(memo->Find(rx[7],7)==rw[7]);
 assert(mprotect(storage,static_cast<std::size_t>(page),PROT_READ|PROT_WRITE)==0);
 assert(memo->Find(rx[0],7)==rw[0]);
 assert(mprotect(storage,static_cast<std::size_t>(page),PROT_READ)==0);
 for(unsigned i=0;i<10000;++i) assert(memo->Find(rx[0],7)==rw[0]);
 assert(mprotect(storage,static_cast<std::size_t>(page),PROT_READ|PROT_WRITE)==0);
 memo->~AliasPositiveCache();
 assert(munmap(storage,static_cast<std::size_t>(page))==0);
 // With only two live JIT arenas, alternating hits also remain read-only.
 storage=mmap(nullptr,static_cast<std::size_t>(page),PROT_READ|PROT_WRITE,
              MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
 assert(storage!=MAP_FAILED);
 memo=new(storage) Eden::Jit::AliasPositiveCache{};
 memo->Remember(rx[0],rw[0],8);memo->Remember(rx[1],rw[1],8);
 assert(mprotect(storage,static_cast<std::size_t>(page),PROT_READ)==0);
 for(unsigned i=0;i<20000;++i)
   assert(memo->Find(rx[i%2],8)==rw[i%2]);
 assert(mprotect(storage,static_cast<std::size_t>(page),PROT_READ|PROT_WRITE)==0);
 memo->~AliasPositiveCache();
 assert(munmap(storage,static_cast<std::size_t>(page))==0);
 assert(!aliases.Find(rx[0],8)); // trim/release epoch flush
 assert(aliases.Find(rx[3],7)==rw[3]);
 aliases.Remember(rx[0],rw[0],8);
 assert(aliases.Find(rx[0],8)==rw[0]);
 assert(!aliases.Find(rx[3],8));
 constexpr std::size_t size=2u*1024u*1024u;
 auto* a=allocator->alloc(size);auto* b=allocator->alloc(size);
 assert(a && b && a!=b);
 auto* aw=allocator->writableAddress(a);auto* bw=allocator->writableAddress(b);
 assert(aw && bw && aw!=a && bw!=b);
 std::memset(aw,0xA5,size);std::memset(bw,0x5A,size);
 assert(a[0]==0xA5 && a[size-1]==0xA5);
 assert(b[0]==0x5A && b[size-1]==0x5A);
 // Adding a third arena must preserve the two existing RX/RW aliases;
 // its subsequent retirement must still invalidate stale TLS hints.
 for(unsigned cycle=0;cycle<8;++cycle){
   auto* extra=allocator->alloc(size);
   assert(extra && extra!=a && extra!=b);
   auto* extra_rw=allocator->writableAddress(extra);
   assert(extra_rw && extra_rw!=extra);
   std::memset(extra_rw,0x6B,size);
   assert(extra[0]==0x6B && extra[size-1]==0x6B);
   assert(allocator->writableAddress(a)==aw);
   assert(allocator->writableAddress(b)==bw);
   allocator->free(extra);
   assert(allocator->writableAddress(extra)==extra);
 }
 for(unsigned i=0;i<100000;++i){
  assert(allocator->writableAddress(a)==aw);
  assert(allocator->writableAddress(b)==bw);
 }
 std::thread worker([&]{
  for(unsigned i=0;i<10000;++i){
   assert(allocator->writableAddress(a)==aw);
   assert(allocator->writableAddress(b)==bw);
  }
 });worker.join();
 allocator->free(a);
 assert(allocator->writableAddress(a)==a);
 auto* c=allocator->alloc(size);assert(c);
 auto* cw=allocator->writableAddress(c);assert(cw && cw!=c);
 std::memset(cw,0x3C,size);assert(c[0]==0x3C && c[size-1]==0x3C);
 assert(allocator->writableAddress(b)==bw);
 allocator->free(c);allocator->free(b);
 assert(allocator->writableAddress(c)==c);
 assert(allocator->writableAddress(b)==b);
}
"""
with tempfile.TemporaryDirectory(prefix="eden-jit-alias-") as temp:
 d=Path(temp);(d/"xbyak").mkdir()
 (d/"xbyak/xbyak.h").write_text(stub)
 (d/"witness.cpp").write_text(fixture)
 for opt in ("-O0","-O2"):
  exe=d/("witness_"+opt[1:])
  subprocess.run([compiler,"-std=c++20",opt,"-g","-pthread","-Wall","-Wextra",
   "-Werror","-fsanitize=address,undefined","-fno-sanitize-recover=all",
   "-I",str(d),"-I",str(root/"headless"),str(d/"witness.cpp"),"-o",str(exe)],
   check=True,timeout=90)
  subprocess.run([str(exe)],check=True,timeout=90)
print("JIT_ALIAS_LOOKUP_HOST_PASS real_rx_rw=1 alternating=1 collision8=1 eviction9=1 epoch=1 retirement=1 insertion_preserves=1 preferred_readonly=1 dual_readonly=1 sanitizer=1 O0_O2=1")
