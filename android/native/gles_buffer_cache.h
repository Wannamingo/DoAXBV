// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "content_hash.h"
#include <GLES3/gl3.h>
#include <cstring>
#include <unordered_map>
#include <list>
#include <vector>

// Adreno repeats index-range validation when glBufferData replaces a store.
// Keep immutable geometry data on the GPU. Guest pointers are not identities:
// both the complete hash and a byte comparison must match before reuse.
class RecompGlesBufferCache {
    struct Entry {uint64_t key;GLuint name;std::vector<uint8_t> data;};
    std::list<Entry> recent;
    std::unordered_multimap<uint64_t,std::list<Entry>::iterator> entries;
    size_t used=0,byte_limit,entry_limit;GLenum target;
    void evict(){auto item=std::prev(recent.end());auto range=entries.equal_range(item->key);
        for(auto i=range.first;i!=range.second;++i)if(i->second==item){entries.erase(i);break;}
        used-=item->data.size();glDeleteBuffers(1,&item->name);recent.erase(item);
    }
public:
    uint64_t hits=0,uploads=0;
    explicit RecompGlesBufferCache(size_t bytes=16u*1024u*1024u,size_t count=4096,GLenum binding=GL_ELEMENT_ARRAY_BUFFER):byte_limit(bytes),entry_limit(count),target(binding){}
    void clear(){while(!recent.empty())evict();}
    GLuint get(const void *data,size_t bytes){
        if(!data || !bytes || bytes>byte_limit || !entry_limit)return 0;
        uint64_t key=recomp_content_hash(data,bytes);auto range=entries.equal_range(key);
        for(auto i=range.first;i!=range.second;++i)
            if(i->second->data.size()==bytes && !std::memcmp(i->second->data.data(),data,bytes)){++hits;recent.splice(recent.begin(),recent,i->second);return i->second->name;}
        while(used+bytes>byte_limit || entries.size()>=entry_limit)evict();
        Entry entry{};entry.key=key;entry.data.assign((const uint8_t *)data,(const uint8_t *)data+bytes);
        glGenBuffers(1,&entry.name);glBindBuffer(target,entry.name);glBufferData(target,bytes,data,GL_STATIC_DRAW);
        GLuint name=entry.name;used+=bytes;++uploads;recent.push_front(std::move(entry));entries.emplace(key,recent.begin());return name;
    }
};
