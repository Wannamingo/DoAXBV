// SPDX-License-Identifier: GPL-2.0-or-later
#include "gl_shader_source.h"
#include "d3d_draw_model.h"
#include <cstdio>
#include <fstream>
int main(int argc,char **argv)
{
    const unsigned layouts[]={0x002,0x012,0x042,0x102,0x112,0x142,0x144,0x116,0x118,0x11a,0x212,0x242,0x244,0x412,0x442,0x444};
    for(unsigned fvf:layouts){RecompD3dPresenterDrawCommand draw{};draw.fvf=fvf;RecompGlShaderSource shader;std::string error;
        if(!recomp_gl_shader_source(draw,shader,error)){std::fprintf(stderr,"fvf=%x failed: %s\n",fvf,error.c_str());return 1;}
        std::printf("fvf=%x GLSL vertex=%zu fragment=%zu attributes:",fvf,shader.vertex.size(),shader.fragment.size());for(auto &attribute:shader.attributes)std::printf(" %s@%u",attribute.name.c_str(),attribute.location);std::puts("");
        if(argc>1){std::ofstream(std::string(argv[1])+"/"+std::to_string(fvf)+".vert")<<shader.vertex;std::ofstream(std::string(argv[1])+"/"+std::to_string(fvf)+".frag")<<shader.fragment;}
    }
    std::puts("Fixed-function HLSL to GLES shader translation passed.");return 0;
}
