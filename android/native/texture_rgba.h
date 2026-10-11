#pragma once
#include "d3d_texture_model.h"
#include <cstddef>
#include <vector>
bool recomp_texture_rgba(const RecompD3dTextureDesc &,const void *,size_t,const void *,size_t,std::vector<uint8_t> &,size_t &);
