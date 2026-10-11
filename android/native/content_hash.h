// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <cstddef>
#include <cstdint>
#define XXH_INLINE_ALL
#define XXH_NAMESPACE doaxbv_
#include "../third_party/xxhash/xxhash.h"
// Cache identity covers every payload byte, including dynamic movie frames,
// mip levels and palettes. The seed chains palette content to texture content.
inline uint64_t recomp_content_hash(const void *data,size_t bytes,uint64_t seed=14695981039346656037ull)
{ return XXH3_64bits_withSeed(data,bytes,seed); }
