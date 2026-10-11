#include "texture_rgba.h"
#include <algorithm>
#include <cstring>
namespace {
uint16_t u16(const uint8_t *p){return uint16_t(p[0])|uint16_t(p[1])<<8;}
void rgb565(uint16_t v,uint8_t *rgb)
{
    unsigned r=v>>11,g=(v>>5)&63,b=v&31;
    rgb[0]=(r<<3)|(r>>2);rgb[1]=(g<<2)|(g>>4);rgb[2]=(b<<3)|(b>>2);rgb[3]=255;
}
}
bool recomp_texture_rgba(const RecompD3dTextureDesc &desc,const void *data,size_t bytes,
                         const void *palette,size_t palette_bytes,std::vector<uint8_t> &rgba,size_t &consumed)
{
    consumed=0;
    if(!data || !desc.width || !desc.height || desc.width>4096 || desc.height>4096) return false;
    const auto *source=(const uint8_t *)data;
    size_t pixels=size_t(desc.width)*desc.height;
    rgba.assign(pixels*4,0);
    bool bc=desc.format_byte==0x0c || desc.format_byte==0x0e || desc.format_byte==0x0f;
    if(bc) {
        size_t block_bytes=desc.format_byte==0x0c ? 8 : 16;
        size_t columns=(desc.width+3)/4,rows=(desc.height+3)/4;
        consumed=columns*rows*block_bytes;
        if(consumed>bytes) return false;
        for(size_t by=0;by<rows;++by) for(size_t bx=0;bx<columns;++bx) {
            const uint8_t *block=source+(by*columns+bx)*block_bytes;
            const uint8_t *color=block+(block_bytes==8 ? 0 : 8);
            uint8_t colors[4][4];uint16_t c0=u16(color),c1=u16(color+2);
            rgb565(c0,colors[0]);rgb565(c1,colors[1]);
            if(c0>c1 || block_bytes==16) {
                for(unsigned lane=0;lane<3;++lane) {
                    colors[2][lane]=(2*colors[0][lane]+colors[1][lane])/3;
                    colors[3][lane]=(colors[0][lane]+2*colors[1][lane])/3;
                }
                colors[2][3]=colors[3][3]=255;
            } else {
                for(unsigned lane=0;lane<3;++lane) colors[2][lane]=(colors[0][lane]+colors[1][lane])/2;
                colors[2][3]=255;std::memset(colors[3],0,4);
            }
            uint8_t alpha[8]{};uint64_t alpha_bits=0;
            if(desc.format_byte==0x0f) {
                alpha[0]=block[0];alpha[1]=block[1];
                if(alpha[0]>alpha[1]) for(unsigned i=2;i<8;++i) alpha[i]=((8-i)*alpha[0]+(i-1)*alpha[1])/7;
                else { for(unsigned i=2;i<6;++i) alpha[i]=((6-i)*alpha[0]+(i-1)*alpha[1])/5;alpha[6]=0;alpha[7]=255; }
                for(unsigned i=0;i<6;++i) alpha_bits|=uint64_t(block[i+2])<<(8*i);
            }
            uint32_t selectors=uint32_t(color[4])|uint32_t(color[5])<<8|uint32_t(color[6])<<16|uint32_t(color[7])<<24;
            for(unsigned p=0;p<16;++p) {
                size_t x=bx*4+p%4,y=by*4+p/4;
                if(x>=desc.width || y>=desc.height) continue;
                uint8_t *pixel=rgba.data()+(y*desc.width+x)*4;
                std::memcpy(pixel,colors[(selectors>>(p*2))&3],4);
                if(desc.format_byte==0x0e) pixel[3]=((block[p/2]>>((p&1)*4))&15)*17;
                if(desc.format_byte==0x0f) pixel[3]=alpha[(alpha_bits>>(p*3))&7];
            }
        }
        return true;
    }
    unsigned texel_bytes=(desc.format_byte==0x0b || desc.format_byte==0x19) ? 1 : 4;
    if(desc.format_byte!=0x0b && desc.format_byte!=0x19 && desc.format_byte!=0x06 && desc.format_byte!=0x12) return false;
    if(desc.format_byte==0x0b && (!palette || palette_bytes<1024)) return false;
    size_t pitch=desc.linear ? desc.pitch : desc.width*texel_bytes;
    if(pitch<desc.width*texel_bytes) return false;
    consumed=desc.linear ? pitch*(desc.height-1)+desc.width*texel_bytes : pixels*texel_bytes;
    if(consumed>bytes) return false;
    std::vector<uint8_t> unpacked;
    if(!desc.linear) {
        unpacked.resize(pixels*texel_bytes);
        if(!recomp_d3d_texture_unswizzle(source,unpacked.data(),desc.width,desc.height,texel_bytes)) return false;
        source=unpacked.data();
    }
    for(unsigned y=0;y<desc.height;++y) for(unsigned x=0;x<desc.width;++x) {
        const uint8_t *input=source+y*pitch+x*texel_bytes;
        uint8_t *output=rgba.data()+(size_t(y)*desc.width+x)*4;
        if(desc.format_byte==0x0b) input=(const uint8_t *)palette+*input*4;
        if(desc.format_byte==0x19){output[0]=output[1]=output[2]=255;output[3]=*input;}
        else {output[0]=input[2];output[1]=input[1];output[2]=input[0];output[3]=input[3];}
    }
    return true;
}
