// SPDX-License-Identifier: GPL-2.0-or-later
#include "content_hash.h"
#include <cstdio>
#include <vector>
#include <chrono>
static uint64_t previous_hash(const void *data,size_t bytes,uint64_t seed)
{const auto *p=(const uint8_t *)data;for(size_t i=0;i<bytes;++i)seed=(seed^p[i])*1099511628211ull;return seed;}
int main()
{
    std::vector<uint8_t> data(2*1024*1024+16);
    for(size_t i=0;i<data.size();++i)data[i]=uint8_t(i*17+(i>>8));
    // Mutations at every byte of short/aligned and unaligned buffers catch
    // accidental sampling or forgotten tails; larger probes cover cache data.
    for(unsigned alignment=0;alignment<16;++alignment)for(unsigned size=1;size<=257;++size){
        auto *p=data.data()+alignment;const auto before=recomp_content_hash(p,size);
        for(unsigned offset=0;offset<size;++offset){p[offset]^=128;auto changed=recomp_content_hash(p,size);p[offset]^=128;if(changed==before)return 1;}
    }
    for(size_t size:{4096u,65536u,2u*1024u*1024u}){
        auto before=recomp_content_hash(data.data(),size);
        for(size_t offset:{size_t(0),size/3,size/2,size-2,size-1}){data[offset]^=1;auto changed=recomp_content_hash(data.data(),size);data[offset]^=1;if(changed==before)return 2;}
        uint8_t palette[1024]{};auto palette_before=recomp_content_hash(palette,sizeof palette,before);palette[997]=1;if(recomp_content_hash(palette,sizeof palette,before)==palette_before)return 3;
    }
    volatile uint64_t checksum=0;
    auto measure=[&](bool fast){auto start=std::chrono::steady_clock::now();for(unsigned i=0;i<128;++i)checksum^=fast ? recomp_content_hash(data.data(),2*1024*1024,i):previous_hash(data.data(),2*1024*1024,i);return std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();};
    auto old_ms=measure(false),new_ms=measure(true);
    std::printf("Full-content hash: mutations, tails, unaligned buffers and palettes passed. FNV=%.2fms XXH3=%.2fms ratio=%.2f checksum=%llu\n",old_ms,new_ms,old_ms/new_ms,(unsigned long long)checksum);
    return 0;
}
