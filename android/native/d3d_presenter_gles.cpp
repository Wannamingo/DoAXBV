// SPDX-License-Identifier: GPL-2.0-or-later
// Android implementation of the existing API-level presenter seam.
#include "d3d_presenter.h"
#include "d3d_draw_model.h"
#include "gl_shader_source.h"
#include "texture_rgba.h"
#include "content_hash.h"
#include "gles_buffer_cache.h"
#include <SDL3/SDL.h>
#include <GLES3/gl3.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <new>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace {
bool immediate;
std::atomic<float> visible_fps{};
std::atomic<uint64_t> fps_updated_ms{};
// UI threads publish requests; all GL changes run on the presenter owner.
std::atomic<unsigned> requested_display{},display_failure{};
std::atomic<uint64_t> presented_frames{};
struct Surface { GLuint color=0,fbo=0; unsigned width=0,height=0,pixel_width=0,pixel_height=0; };
struct Depth { GLuint buffer=0; unsigned width=0,height=0,pixel_width=0,pixel_height=0; };
struct Texture { GLuint name=0; uint64_t fingerprint=0; size_t bytes=0; };
struct Pipeline { GLuint name=0; std::vector<RecompGlAttribute> attributes; };
uint64_t hash(const void *source,size_t bytes,uint64_t value=14695981039346656037ull)
{ return recomp_content_hash(source,bytes,value); }
std::string texture_key(const RecompD3dTextureDesc &d)
{ return std::to_string(d.data)+":"+std::to_string(d.width)+":"+std::to_string(d.height)+":"+std::to_string(d.format_byte)+":"+std::to_string(d.pitch)+":"+std::to_string(d.mip_levels); }
GLuint shader(GLenum type,const std::string &source)
{
    GLuint result=glCreateShader(type);const char *text=source.c_str();glShaderSource(result,1,&text,nullptr);glCompileShader(result);
    GLint okay=0;glGetShaderiv(result,GL_COMPILE_STATUS,&okay);
    if(!okay){GLint size=0;glGetShaderiv(result,GL_INFO_LOG_LENGTH,&size);std::string log(std::max(1,size),'\0');glGetShaderInfoLog(result,size,nullptr,log.data());std::fprintf(stderr,"GLES shader: %s\n%s\n",log.c_str(),source.c_str());glDeleteShader(result);return 0;}return result;
}
GLuint link(const std::string &vertex,const std::string &fragment)
{
    GLuint vs=shader(GL_VERTEX_SHADER,vertex),ps=shader(GL_FRAGMENT_SHADER,fragment);
    if(!vs || !ps){if(vs)glDeleteShader(vs);if(ps)glDeleteShader(ps);return 0;}
    GLuint result=glCreateProgram();glAttachShader(result,vs);glAttachShader(result,ps);glLinkProgram(result);glDeleteShader(vs);glDeleteShader(ps);
    GLint okay=0;glGetProgramiv(result,GL_LINK_STATUS,&okay);
    if(!okay){GLint size=0;glGetProgramiv(result,GL_INFO_LOG_LENGTH,&size);std::string log(std::max(1,size),'\0');glGetProgramInfoLog(result,size,nullptr,log.data());std::fprintf(stderr,"GLES link: %s\n",log.c_str());glDeleteProgram(result);return 0;}return result;
}
unsigned target_scale(unsigned preferred,unsigned width,unsigned height)
{return std::min(preferred,4096u/std::max(width,height));}
bool surface(Surface &s,unsigned width,unsigned height,unsigned scale=1)
{
    if(!width || !height || width>4096 || height>4096)return false;
    scale=target_scale(scale,width,height);s.width=width;s.height=height;s.pixel_width=width*scale;s.pixel_height=height*scale;glGenTextures(1,&s.color);glBindTexture(GL_TEXTURE_2D,s.color);
    glTexStorage2D(GL_TEXTURE_2D,1,GL_RGBA8,s.pixel_width,s.pixel_height);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    glGenFramebuffers(1,&s.fbo);glBindFramebuffer(GL_FRAMEBUFFER,s.fbo);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,s.color,0);
    if(glCheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE)return false;
    glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);glClearColor(0,0,0,0);glClear(GL_COLOR_BUFFER_BIT);return true;
}
void destroy(Surface &s){glDeleteFramebuffers(1,&s.fbo);glDeleteTextures(1,&s.color);s={};}
GLenum compare(unsigned index){static const GLenum table[]={GL_NEVER,GL_LESS,GL_EQUAL,GL_LEQUAL,GL_GREATER,GL_NOTEQUAL,GL_GEQUAL,GL_ALWAYS};return table[index<8 ? index : 0];}
GLenum stencil(unsigned index){static const GLenum table[]={GL_KEEP,GL_ZERO,GL_REPLACE,GL_INCR,GL_DECR,GL_INVERT,GL_INCR_WRAP,GL_DECR_WRAP};return table[index<8 ? index : 0];}
GLenum factor(unsigned index){static const GLenum table[]={GL_ZERO,GL_ONE,GL_SRC_COLOR,GL_ONE_MINUS_SRC_COLOR,GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA,GL_DST_ALPHA,GL_ONE_MINUS_DST_ALPHA,GL_DST_COLOR,GL_ONE_MINUS_DST_COLOR,GL_SRC_ALPHA_SATURATE,GL_CONSTANT_COLOR,GL_ONE_MINUS_CONSTANT_COLOR};return table[index<13 ? index : 0];}
void color(uint32_t argb,float *rgba){rgba[0]=((argb>>16)&255)/255.f;rgba[1]=((argb>>8)&255)/255.f;rgba[2]=(argb&255)/255.f;rgba[3]=(argb>>24)/255.f;}
}
struct RecompD3dPresenter {
    SDL_Window *window=nullptr;SDL_GLContext context=nullptr;std::thread::id owner;
    Surface main,feedback;Surface *current=nullptr;GLuint vao=0,vertices=0,indices=0,constants=0,blit=0,gamma=0,white=0;
    std::unordered_map<uint32_t,Surface> targets;std::unordered_map<uint32_t,Depth> depths;
    std::unordered_map<std::string,Texture> textures;std::unordered_map<std::string,Pipeline> pipelines;
    GLuint attribute_program=0,attribute_buffer=0;unsigned attribute_stride=0;uint32_t enabled_attributes=0;
    std::array<float,1048> previous_constants{};bool constants_valid=false;
    bool adreno=false;RecompGlesBufferCache index_cache;
    RecompGlesBufferCache vertex_cache{64u*1024u*1024u,4096,GL_ARRAY_BUFFER};
    uint64_t uniform_ticks=0,vertex_ticks=0,index_ticks=0,issue_ticks=0;
    std::vector<uint8_t> converted_vertices;
    RecompD3dPresenterTarget previous_target{};bool target_valid=false;
    RecompD3dDepthState previous_depth{};RecompD3dBlendState previous_blend{};
    RecompD3dCullMode previous_cull{};bool depth_valid=false,blend_valid=false,cull_valid=false;
    unsigned render_scale=1;
    unsigned rate_frames=0;
    double display_aspect=0.0;const char *display_profile="source";
    uint64_t target_bytes=0,texture_bytes=0,frames=0,draws=0,declines=0,rate_start=0,draw_ticks=0,texture_ticks=0;bool border=false,closed=false;
    std::array<uint64_t,256> sampled{},unsupported{};
};
namespace {
bool target(RecompD3dPresenter &p,const RecompD3dPresenterTarget &t)
{
    // Draws usually share the same color/depth target. Avoid rebinding and
    // rechecking a complete FBO for each of the scene's thousands of draws.
    // Invalidate before any change so a rejected target cannot leave stale state.
    if(p.target_valid && p.current && !std::memcmp(&p.previous_target,&t,sizeof t))return true;
    p.target_valid=false;
    Surface *s=&p.main;
    if(t.offscreen){const auto &d=t.color;if(!d.data || d.depth || (d.format_byte!=6 && d.format_byte!=0x12))return false;
        auto found=p.targets.find(d.data);
        if(found!=p.targets.end() && (found->second.width!=d.width || found->second.height!=d.height)){
            p.target_bytes-=size_t(found->second.pixel_width)*found->second.pixel_height*4;destroy(found->second);p.targets.erase(found);found=p.targets.end();}
        if(found==p.targets.end()){
            if(!d.width||!d.height||d.width>4096||d.height>4096)return false;unsigned scale=target_scale(p.render_scale,d.width,d.height);
            size_t bytes=size_t(d.width)*d.height*4*scale*scale;if(bytes>64u*1024u*1024u*p.render_scale*p.render_scale-p.target_bytes)return false;
            Surface created;p.blend_valid=false;if(!surface(created,d.width,d.height,p.render_scale)){destroy(created);return false;}p.target_bytes+=bytes;found=p.targets.emplace(d.data,created).first;}
        s=&found->second;
    }
    glBindFramebuffer(GL_FRAMEBUFFER,s->fbo);GLuint depth=0;
    if(!t.no_depth){uint32_t key=t.custom_depth ? t.depth.data : 0;
        unsigned width=t.custom_depth ? t.depth.width : p.main.width,height=t.custom_depth ? t.depth.height : p.main.height;
        if(width<s->width || height<s->height || (t.custom_depth && (!key || t.depth.format_byte!=0x2e)))return false;
        if(!width||!height||width>4096||height>4096)return false;unsigned scale=target_scale(p.render_scale,width,height),pixel_width=width*scale,pixel_height=height*scale;
        if(pixel_width<s->pixel_width||pixel_height<s->pixel_height)return false;
        auto &d=p.depths[key];
        if(d.buffer && (d.width!=width || d.height!=height)){glDeleteRenderbuffers(1,&d.buffer);d={};}
        if(!d.buffer){d.width=width;d.height=height;d.pixel_width=pixel_width;d.pixel_height=pixel_height;glGenRenderbuffers(1,&d.buffer);glBindRenderbuffer(GL_RENDERBUFFER,d.buffer);glRenderbufferStorage(GL_RENDERBUFFER,GL_DEPTH24_STENCIL8,pixel_width,pixel_height);}
        depth=d.buffer;
    }
    glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_STENCIL_ATTACHMENT,GL_RENDERBUFFER,depth);
    if(glCheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE)return false;
    p.current=s;glViewport(0,0,s->pixel_width,s->pixel_height);p.previous_target=t;p.target_valid=true;return true;
}
GLuint copy_target(RecompD3dPresenter &p,Surface &source)
{
    p.target_valid=false;
    if(p.feedback.width!=source.width || p.feedback.height!=source.height){destroy(p.feedback);p.blend_valid=false;if(!surface(p.feedback,source.width,source.height,source.pixel_width/source.width))return 0;}
    glBindFramebuffer(GL_READ_FRAMEBUFFER,source.fbo);glBindFramebuffer(GL_DRAW_FRAMEBUFFER,p.feedback.fbo);
    glBlitFramebuffer(0,0,source.pixel_width,source.pixel_height,0,0,p.feedback.pixel_width,p.feedback.pixel_height,GL_COLOR_BUFFER_BIT,GL_NEAREST);
    glBindFramebuffer(GL_FRAMEBUFFER,p.current->fbo);glViewport(0,0,p.current->pixel_width,p.current->pixel_height);return p.feedback.color;
}
GLuint texture(RecompD3dPresenter &p,const RecompD3dTextureDesc &d,const void *bytes,size_t count,const void *palette,size_t palette_count,bool backbuffer=false)
{
    if(backbuffer)return copy_target(p,p.main);
    auto rendered=p.targets.find(d.data);
    if(rendered!=p.targets.end() && rendered->second.width==d.width && rendered->second.height==d.height){return &rendered->second==p.current ? copy_target(p,rendered->second) : rendered->second.color;}
    if(!bytes || !count)return 0;
    auto key=texture_key(d);uint64_t fingerprint=hash(bytes,count);if(palette && palette_count)fingerprint=hash(palette,palette_count,fingerprint);
    auto found=p.textures.find(key);if(found!=p.textures.end() && found->second.fingerprint==fingerprint)return found->second.name;
    if(found!=p.textures.end()){p.texture_bytes-=found->second.bytes;glDeleteTextures(1,&found->second.name);p.textures.erase(found);}
    bool bc=d.format_byte==0x0c || d.format_byte==0x0e || d.format_byte==0x0f;
    unsigned levels=bc ? std::max(1u,d.mip_levels) : 1;
    if(levels>13)return 0;
    size_t required=0;for(unsigned i=0;i<levels;++i)required+=size_t(std::max(1u,d.width>>i))*std::max(1u,d.height>>i)*4;
    // Guest RAM bounds the live data, but decoded BC textures require more space.
    if(required>256u*1024u*1024u)return 0;
    if(p.texture_bytes+required>256u*1024u*1024u || p.textures.size()>=4096){for(auto &entry:p.textures)glDeleteTextures(1,&entry.second.name);p.textures.clear();p.texture_bytes=0;}
    Texture created;glGenTextures(1,&created.name);glBindTexture(GL_TEXTURE_2D,created.name);glPixelStorei(GL_UNPACK_ALIGNMENT,1);
    size_t offset=0;std::vector<uint8_t> rgba;
    for(unsigned i=0;i<levels;++i){auto level=d;level.width=std::max(1u,d.width>>i);level.height=std::max(1u,d.height>>i);size_t consumed=0;
        if(offset>=count || !recomp_texture_rgba(level,(const uint8_t *)bytes+offset,count-offset,palette,palette_count,rgba,consumed)){glDeleteTextures(1,&created.name);return 0;}
        glTexImage2D(GL_TEXTURE_2D,i,GL_RGBA8,level.width,level.height,0,GL_RGBA,GL_UNSIGNED_BYTE,rgba.data());offset+=consumed;}
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAX_LEVEL,levels-1);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,levels>1 ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    created.fingerprint=fingerprint;created.bytes=required;p.texture_bytes+=required;p.textures.emplace(key,created);return created.name;
}
bool wrap(RecompD3dPresenter &p,unsigned unit,GLuint name,unsigned u,unsigned v)
{
    auto mode=[&](unsigned x)->GLenum{switch(x){case 2:return GL_MIRRORED_REPEAT;case 3:return GL_CLAMP_TO_EDGE;case 4:return p.border ? 0x812d : 0;default:return GL_REPEAT;}};
    GLenum a=mode(u),b=mode(v);if(!a || !b)return false;
    glActiveTexture(GL_TEXTURE0+unit);glBindTexture(GL_TEXTURE_2D,name ? name : p.white);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,a);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,b);
    if(u==4 || v==4){const GLfloat transparent[4]={};glTexParameterfv(GL_TEXTURE_2D,0x1004,transparent);}return true;
}
Pipeline *pipeline(RecompD3dPresenter &p,const RecompD3dPresenterDrawCommand &d)
{
    std::string key((const char *)&d.fvf,sizeof d.fvf);key.append((const char *)d.program,size_t(d.program_count)*16);
    auto found=p.pipelines.find(key);if(found!=p.pipelines.end())return &found->second;
    RecompGlShaderSource source;std::string error;if(!recomp_gl_shader_source(d,source,error)){std::fprintf(stderr,"GLES translation fvf=%x: %s\n",d.fvf,error.c_str());return nullptr;}
    Pipeline created;created.name=link(source.vertex,source.fragment);if(!created.name)return nullptr;created.attributes=std::move(source.attributes);
    GLuint block=glGetUniformBlockIndex(created.name,"Transform");if(block==GL_INVALID_INDEX){std::fprintf(stderr,"GLES missing Transform uniform block\n");glDeleteProgram(created.name);return nullptr;}
    GLint size=0;glGetActiveUniformBlockiv(created.name,block,GL_UNIFORM_BLOCK_DATA_SIZE,&size);if(size!=1048*4){std::fprintf(stderr,"GLES unexpected Transform size %d\n",size);glDeleteProgram(created.name);return nullptr;}glUniformBlockBinding(created.name,block,0);
    glUseProgram(created.name);for(auto &sampler:source.samplers){GLint location=glGetUniformLocation(created.name,sampler.first.c_str());if(location>=0)glUniform1i(location,sampler.second);}
    std::fprintf(stderr,"GLES pipeline fvf=%x instructions=%u\n",d.fvf,d.program_count);return &p.pipelines.emplace(std::move(key),std::move(created)).first->second;
}
bool constants(const RecompD3dPresenterDrawCommand &d,const RecompD3dVertexLayout &layout,const Surface &surface,bool textured,float *c)
{
    std::fill(c,c+1048,0.f);std::memcpy(c,d.transform,64);std::memcpy(c+16,d.blend_transforms,192);
    if(layout.pretransformed || d.program_count){std::fill(c,c+16,0.f);c[0]=2.f/surface.width;c[5]=-2.f/surface.height;c[10]=1;c[12]=-1+.5f*c[0];c[13]=1+.5f*c[5];c[15]=1;}
    if(d.program_count){std::memcpy(c+140,d.program_constants,sizeof d.program_constants);for(unsigned axis=0;axis<2;++axis){float scale=d.program_constants[58][axis];if(!std::isfinite(scale) || !scale)return false;c[axis*5]=1/scale;c[12+axis]=-d.program_constants[59][axis]/scale;}
        float scale=d.program_constants[58][2];if(!std::isfinite(scale) || scale<=0)return false;c[10]=1/scale;c[14]=-d.program_constants[59][2]/scale;c[138]=d.program_alpha_mask ? 1.f:0.f;c[139]=d.program_mask_lod_bias;}
    if(d.directional.enabled && !d.program_count){const auto &l=d.directional;if(l.count>8)return false;float *v=c+908;std::memcpy(v,l.normal_transforms,256);std::memcpy(v+64,l.ambient_emissive,16);std::memcpy(v+68,l.material_diffuse,16);std::memcpy(v+72,l.directions,128);std::memcpy(v+104,l.colors,128);v[136]=1;v[137]=l.normalize ? 1.f:0.f;v[138]=float(l.count);}
    c[64]=textured ? 1.f:0.f;c[65]=d.depth.alpha_test_enable ? 1.f:0.f;c[66]=float(d.depth.alpha_func);c[67]=float(d.depth.alpha_ref);c[68]=d.blend_weight_count ? 1.f:0.f;c[69]=d.use_texture_factor ? 1.f:(d.modulate_texture_factor ? 2.f:0.f);c[70]=d.material_alpha_mode==RECOMP_D3D_MATERIAL_ALPHA_NONE ? 1.f:d.material_alpha;c[71]=d.material_alpha_mode==RECOMP_D3D_MATERIAL_ALPHA_SELECT_DIFFUSE ? 1.f:0.f;color(d.texture_factor,c+72);c[76]=d.zero_diffuse_rgb ? 1.f:0.f;c[77]=d.texture.format_byte==0x19 ? 1.f:0.f;
    c[78]=d.alpha_mask.linear && d.alpha_mask.width ? 1.f/d.alpha_mask.width:1.f;c[79]=d.alpha_mask.linear && d.alpha_mask.height ? 1.f/d.alpha_mask.height:1.f;c[80]=d.texture.linear && d.texture.width ? 1.f/d.texture.width:1.f;c[81]=d.texture.linear && d.texture.height ? 1.f/d.texture.height:1.f;c[82]=d.four_tap_filter ? 1.f:0.f;c[83]=d.has_alpha_mask ? 1.f:0.f;
    if(d.has_reflection){std::memcpy(c+84,d.reflection_world_view,64);std::memcpy(c+100,d.reflection_normal,64);std::memcpy(c+116,d.reflection_transform,64);std::memcpy(c+132,d.reflection_diffuse,16);c[136]=d.reflection_mesh_uv ? 2.f:1.f;c[137]=d.reflection_normalize ? 1.f:0.f;}return true;
}
RecompD3dPresenterError draw(RecompD3dPresenter &p,const RecompD3dPresenterDrawCommand &original)
{
    const uint64_t started=SDL_GetPerformanceCounter();
    auto d=original;d.fvf=recomp_d3d_fvf_for_stream(d.fvf,d.vertex_stride);RecompD3dVertexLayout layout{};
    if(d.program_count>136 || !recomp_d3d_fvf_layout(d.fvf,&layout) || layout.stride>d.vertex_stride || !d.vertex_bytes || !d.index_bytes || !d.vertex_count || !d.index_count)return RECOMP_D3D_PRESENTER_UNSUPPORTED_COMMAND;
    if(!target(p,d.target))return RECOMP_D3D_PRESENTER_UNSUPPORTED_COMMAND;
    Pipeline *shader=pipeline(p,d);if(!shader)return RECOMP_D3D_PRESENTER_UNSUPPORTED_COMMAND;
    const uint64_t texture_started=SDL_GetPerformanceCounter();
    GLuint tex=d.has_texture ? texture(p,d.texture,d.texture_bytes,d.texture_byte_count,d.palette_bytes,d.palette_byte_count,d.texture_is_backbuffer):0,mask=0;
    if(d.has_alpha_mask)mask=texture(p,d.alpha_mask,d.alpha_mask_bytes,d.alpha_mask_byte_count,d.alpha_mask_palette,d.alpha_mask_palette_byte_count);
    else if(d.has_reflection || d.program_alpha_mask)mask=texture(p,d.reflection_texture,d.reflection_bytes,d.reflection_byte_count,nullptr,0);
    p.texture_ticks+=SDL_GetPerformanceCounter()-texture_started;
    if(d.has_texture){auto &count=tex ? p.sampled : p.unsupported;if(d.texture.format_byte<256)++count[d.texture.format_byte];}
    if((d.has_texture && !tex) || ((d.has_alpha_mask || d.has_reflection || d.program_alpha_mask) && !mask))return RECOMP_D3D_PRESENTER_UNSUPPORTED_COMMAND;
    float c[1048];if(!constants(d,layout,*p.current,tex!=0,c))return RECOMP_D3D_PRESENTER_UNSUPPORTED_COMMAND;
    if(layout.pretransformed){static std::vector<uint64_t> logged;uint64_t key=(uint64_t(d.fvf)<<32)|d.texture.data;if(logged.size()<24 && std::find(logged.begin(),logged.end(),key)==logged.end()){logged.push_back(key);std::fprintf(stderr,"GLES UI draw fvf=%x tex=%u data=%x fmt=%x has_tex=%u flags=%g factor=%g alpha=%g blend=%u,%u,%u\n",d.fvf,tex,d.texture.data,d.texture.format_byte,unsigned(d.has_texture),c[64],c[69],c[70],unsigned(d.blend.blend_enable),unsigned(d.blend.src_factor),unsigned(d.blend.dst_factor));}}
    uint64_t phase_started=p.adreno ? SDL_GetPerformanceCounter():0;
    glUseProgram(shader->name);glBindBuffer(GL_UNIFORM_BUFFER,p.constants);
    if(!p.constants_valid || std::memcmp(p.previous_constants.data(),c,sizeof c)){
        // A new store lets Adreno retain constants referenced by queued draws.
        // Mali keeps its validated update path and shared VS/FS block instance.
        if(p.adreno)glBufferData(GL_UNIFORM_BUFFER,sizeof c,c,GL_STREAM_DRAW);
        else glBufferSubData(GL_UNIFORM_BUFFER,0,sizeof c,c);
        std::memcpy(p.previous_constants.data(),c,sizeof c);p.constants_valid=true;
    }
    glBindBufferBase(GL_UNIFORM_BUFFER,0,p.constants);
    if(p.adreno){uint64_t now=SDL_GetPerformanceCounter();p.uniform_ticks+=now-phase_started;phase_started=now;}
    glBindVertexArray(p.vao);glBindBuffer(GL_ARRAY_BUFFER,p.vertices);
    size_t bytes=size_t(d.vertex_count)*d.vertex_stride;auto &converted=p.converted_vertices;
    const void *vertices=d.vertex_bytes;if(layout.diffuse_offset!=RECOMP_D3D_FVF_ABSENT){converted.assign((const uint8_t *)vertices,(const uint8_t *)vertices+bytes);for(unsigned i=0;i<d.vertex_count;++i)std::swap(converted[size_t(i)*d.vertex_stride+layout.diffuse_offset],converted[size_t(i)*d.vertex_stride+layout.diffuse_offset+2]);vertices=converted.data();}
    GLuint vertex_buffer=p.adreno ? p.vertex_cache.get(vertices,bytes):0;
    if(vertex_buffer)glBindBuffer(GL_ARRAY_BUFFER,vertex_buffer);
    else {vertex_buffer=p.vertices;glBindBuffer(GL_ARRAY_BUFFER,vertex_buffer);glBufferData(GL_ARRAY_BUFFER,bytes,vertices,GL_STREAM_DRAW);}
    if(p.adreno){uint64_t now=SDL_GetPerformanceCounter();p.vertex_ticks+=now-phase_started;phase_started=now;}
    // Orphaning the VBO preserves its object name and VAO attribute bindings.
    // Presentation uses gl_VertexID and doesn't change these bindings either.
    // Reconfigure only when the actual program layout or stride changes.
    if(p.attribute_program!=shader->name || p.attribute_stride!=d.vertex_stride || p.attribute_buffer!=vertex_buffer){
    p.attribute_program=0;
    uint32_t wanted=0;for(const auto &attribute:shader->attributes){if(attribute.location>=16)return RECOMP_D3D_PRESENTER_UNSUPPORTED_COMMAND;wanted|=1u<<attribute.location;}
    for(unsigned i=0;i<16;++i)if((p.enabled_attributes&~wanted)&(1u<<i)){glDisableVertexAttribArray(i);p.enabled_attributes&=~(1u<<i);}
    for(const auto &attribute:shader->attributes){const std::string &name=attribute.name;unsigned offset=RECOMP_D3D_FVF_ABSENT,components=0;GLenum type=GL_FLOAT;GLboolean normalized=GL_FALSE;
        if(name.find("position")!=std::string::npos){offset=layout.position_offset;components=layout.pretransformed ? 4:3;}
        else if(name.find("weights")!=std::string::npos){offset=12;components=layout.blend_weight_count;}
        else if(name.find("normal")!=std::string::npos){offset=layout.normal_offset;components=3;}
        else if(name.find("diffuse")!=std::string::npos){offset=layout.diffuse_offset;components=4;type=GL_UNSIGNED_BYTE;normalized=GL_TRUE;}
        else if(name.find("texcoord")!=std::string::npos){unsigned index=0;char last=name.back();if(last>='0' && last<='3')index=last-'0';if(index<layout.texcoord_count)offset=layout.texcoord_offset+index*8;components=2;}
        if(offset==RECOMP_D3D_FVF_ABSENT || !components || attribute.location>=16){std::fprintf(stderr,"GLES unknown attribute %s\n",name.c_str());return RECOMP_D3D_PRESENTER_UNSUPPORTED_COMMAND;}
        if(!(p.enabled_attributes&(1u<<attribute.location))){glEnableVertexAttribArray(attribute.location);p.enabled_attributes|=1u<<attribute.location;}
        glVertexAttribPointer(attribute.location,components,type,normalized,d.vertex_stride,(const void *)(uintptr_t)offset);}
    p.enabled_attributes=wanted;p.attribute_program=shader->name;p.attribute_stride=d.vertex_stride;p.attribute_buffer=vertex_buffer;
    }
    GLuint cached_indices=p.adreno ? p.index_cache.get(d.index_bytes,size_t(d.index_count)*2):0;
    if(cached_indices)glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,cached_indices);
    else {glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,p.indices);glBufferData(GL_ELEMENT_ARRAY_BUFFER,size_t(d.index_count)*2,d.index_bytes,GL_STREAM_DRAW);}
    if(p.adreno){uint64_t now=SDL_GetPerformanceCounter();p.index_ticks+=now-phase_started;phase_started=now;}
    if(!wrap(p,0,tex,d.address_u,d.address_v) || !wrap(p,1,mask,1,1))return RECOMP_D3D_PRESENTER_UNSUPPORTED_COMMAND;
    // The GLES translation flips clip Y before rasterization. This reverses
    // winding, so invert the D3D11 front-face choice along with that flip.
    if(!p.cull_valid || p.previous_cull!=d.cull_mode){
    if(d.cull_mode==RECOMP_D3D_CULL_NONE)glDisable(GL_CULL_FACE);else{glEnable(GL_CULL_FACE);glCullFace(GL_BACK);glFrontFace(d.cull_mode==RECOMP_D3D_CULL_CLOCKWISE ? GL_CW:GL_CCW);}
    p.previous_cull=d.cull_mode;p.cull_valid=true;}
    if(!p.depth_valid || std::memcmp(&p.previous_depth,&d.depth,sizeof d.depth)){
    if(d.depth.depth_test_enable)glEnable(GL_DEPTH_TEST);else glDisable(GL_DEPTH_TEST);glDepthFunc(compare(d.depth.depth_func));glDepthMask(d.depth.depth_write_enable);
    if(d.depth.stencil_enable)glEnable(GL_STENCIL_TEST);else glDisable(GL_STENCIL_TEST);glStencilFunc(compare(d.depth.stencil_func),d.depth.stencil_ref,d.depth.stencil_read_mask);glStencilMask(d.depth.stencil_write_mask);glStencilOp(stencil(d.depth.stencil_fail),stencil(d.depth.stencil_zfail),stencil(d.depth.stencil_pass));
    p.previous_depth=d.depth;p.depth_valid=true;}
    if(!p.blend_valid || std::memcmp(&p.previous_blend,&d.blend,sizeof d.blend)){
    if(d.blend.blend_enable)glEnable(GL_BLEND);else glDisable(GL_BLEND);static const GLenum equations[]={GL_FUNC_ADD,GL_MIN,GL_MAX,GL_FUNC_SUBTRACT,GL_FUNC_REVERSE_SUBTRACT};if(unsigned(d.blend.op)>=5)return RECOMP_D3D_PRESENTER_UNSUPPORTED_COMMAND;
    glBlendEquation(equations[d.blend.op]);glBlendFunc(factor(d.blend.src_factor),factor(d.blend.dst_factor));float rgba[4];color(d.blend.constant_color,rgba);glBlendColor(rgba[0],rgba[1],rgba[2],rgba[3]);unsigned m=d.blend.color_write_mask;glColorMask(m&1,m&2,m&4,m&8);
    p.previous_blend=d.blend;p.blend_valid=true;}
    GLenum primitive=d.primitive_type==5 ? GL_TRIANGLES:d.primitive_type==6 ? GL_TRIANGLE_STRIP:d.primitive_type==7 ? GL_TRIANGLE_FAN:0;if(!primitive)return RECOMP_D3D_PRESENTER_UNSUPPORTED_COMMAND;
    glDrawElements(primitive,d.index_count,GL_UNSIGNED_SHORT,nullptr);++p.draws;
    if(p.adreno)p.issue_ticks+=SDL_GetPerformanceCounter()-phase_started;
    p.draw_ticks+=SDL_GetPerformanceCounter()-started;return glGetError()==GL_NO_ERROR ? RECOMP_D3D_PRESENTER_OK:RECOMP_D3D_PRESENTER_HOST_FAILURE;
}
bool events(RecompD3dPresenter &p)
{
    SDL_Event e;while(SDL_PollEvent(&e)){if(e.type==SDL_EVENT_QUIT || e.type==SDL_EVENT_WINDOW_CLOSE_REQUESTED)p.closed=true;
        if(e.type==SDL_EVENT_DID_ENTER_BACKGROUND){while(SDL_WaitEvent(&e)){if(e.type==SDL_EVENT_QUIT){p.closed=true;break;}if(e.type==SDL_EVENT_DID_ENTER_FOREGROUND)break;}}}
    return !p.closed;
}
bool resize_surfaces(RecompD3dPresenter &p,unsigned scale)
{
    struct Replacements {
        Surface main;
        std::vector<std::pair<uint32_t,Surface>> targets;
        ~Replacements(){destroy(main);for(auto &entry:targets)destroy(entry.second);}
    } next;
    p.target_valid=p.depth_valid=p.blend_valid=p.cull_valid=false;
    auto copy=[&](Surface &to,const Surface &from){
        glActiveTexture(GL_TEXTURE0);
        if(!surface(to,from.width,from.height,scale)||glGetError()!=GL_NO_ERROR)return false;
        glBindFramebuffer(GL_READ_FRAMEBUFFER,from.fbo);glBindFramebuffer(GL_DRAW_FRAMEBUFFER,to.fbo);
        glBlitFramebuffer(0,0,from.pixel_width,from.pixel_height,0,0,to.pixel_width,to.pixel_height,GL_COLOR_BUFFER_BIT,GL_LINEAR);
        return glGetError()==GL_NO_ERROR;
    };
    try{
        next.targets.reserve(p.targets.size());
        if(!copy(next.main,p.main))return false;
        for(auto &entry:p.targets){
            next.targets.emplace_back(entry.first,Surface{});
            if(!copy(next.targets.back().second,entry.second))return false;
        }
    }catch(const std::bad_alloc &){return false;}
    // Commit only after every color target has been allocated and copied.
    // Depth is regenerated on the next frame; guest state and textures survive.
    destroy(p.main);p.main=next.main;next.main={};p.target_bytes=0;
    for(auto &entry:next.targets){Surface &previous=p.targets.at(entry.first);destroy(previous);previous=entry.second;entry.second={};p.target_bytes+=uint64_t(previous.pixel_width)*previous.pixel_height*4;}
    for(auto &entry:p.depths)glDeleteRenderbuffers(1,&entry.second.buffer);p.depths.clear();
    destroy(p.feedback);p.current=nullptr;p.render_scale=scale;return true;
}
void display_settings(RecompD3dPresenter &p)
{
    unsigned request=requested_display.exchange(0,std::memory_order_acq_rel);if(!request)return;
    unsigned scale=request&3,aspect=request>>2;
    bool okay=scale==p.render_scale||resize_surfaces(p,scale);
    p.display_aspect=aspect==2?16.0/9.0:aspect==1?4.0/3.0:0.0;
    p.display_profile=aspect==2?"16:9":aspect==1?"4:3":"source";
    p.rate_start=SDL_GetPerformanceCounter();p.rate_frames=0;p.draw_ticks=p.texture_ticks=0;
    if(!okay){
        // A failed allocation leaves the old targets intact and play can resume.
        while(glGetError()!=GL_NO_ERROR){}
        display_failure.store(p.render_scale|(aspect<<2),std::memory_order_release);
        std::fprintf(stderr,"Android live resolution failed requested=%u retained=%u\n",scale,p.render_scale);
    }else std::fprintf(stderr,"Android live display resolution=%ux%u scale=%u aspect=%s frame=%llu\n",p.main.pixel_width,p.main.pixel_height,p.render_scale,p.display_profile,(unsigned long long)p.frames);
}
RecompD3dPresenterError present(RecompD3dPresenter &p)
{
    if(!events(p))return RECOMP_D3D_PRESENTER_CLOSED;
    display_settings(p);
    int width=0,height=0;if(!SDL_GetWindowSizeInPixels(p.window,&width,&height))return RECOMP_D3D_PRESENTER_HOST_FAILURE;
    p.target_valid=p.depth_valid=p.blend_valid=p.cull_valid=false;glBindFramebuffer(GL_FRAMEBUFFER,0);glDisable(GL_DEPTH_TEST);glDisable(GL_STENCIL_TEST);glDisable(GL_CULL_FACE);glDisable(GL_BLEND);glColorMask(1,1,1,1);glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT);
    // Fit the complete guest image to the chosen presentation aspect. Guest
    // camera, coordinates and render targets remain in their original format.
    double display_width=p.display_aspect>0.0 ? p.main.height*p.display_aspect : p.main.width;
    double scale=std::min(double(width)/display_width,double(height)/p.main.height);int w=int(display_width*scale),h=int(p.main.height*scale);glViewport((width-w)/2,(height-h)/2,w,h);
    if(p.frames==0)std::fprintf(stderr,"Android presentation viewport=%d,%d,%d,%d screen=%dx%d aspect=%s\n",(width-w)/2,(height-h)/2,w,h,width,height,p.display_profile);
    glUseProgram(p.blit);glBindVertexArray(p.vao);glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,p.main.color);glActiveTexture(GL_TEXTURE1);glBindTexture(GL_TEXTURE_2D,p.gamma);glDrawArrays(GL_TRIANGLES,0,3);
    if(!SDL_GL_SwapWindow(p.window))return RECOMP_D3D_PRESENTER_HOST_FAILURE;++p.frames;presented_frames.store(p.frames,std::memory_order_release);
    const uint64_t now=SDL_GetPerformanceCounter();
    if(!p.rate_start)p.rate_start=now;
    bool report=++p.rate_frames==60;
    if(report){double seconds=double(now-p.rate_start)/SDL_GetPerformanceFrequency();visible_fps.store(float(60/seconds),std::memory_order_relaxed);fps_updated_ms.store(SDL_GetTicks(),std::memory_order_relaxed);std::fprintf(stderr,"GLES performance fps=%.2f draw_ms=%.2f texture_ms=%.2f\n",60/seconds,p.draw_ticks*1000.0/SDL_GetPerformanceFrequency()/60,p.texture_ticks*1000.0/SDL_GetPerformanceFrequency()/60);p.rate_start=now;p.rate_frames=0;p.draw_ticks=p.texture_ticks=0;}
    if(p.adreno && report){double factor=1000.0/SDL_GetPerformanceFrequency()/60;std::fprintf(stderr,"Adreno phases uniform_ms=%.2f vertex_ms=%.2f index_ms=%.2f issue_ms=%.2f index_hits=%llu index_uploads=%llu\n",p.uniform_ticks*factor,p.vertex_ticks*factor,p.index_ticks*factor,p.issue_ticks*factor,(unsigned long long)p.index_cache.hits,(unsigned long long)p.index_cache.uploads);p.uniform_ticks=p.vertex_ticks=p.index_ticks=p.issue_ticks=0;}
    if(p.adreno && report)std::fprintf(stderr,"Adreno vertices hits=%llu uploads=%llu\n",(unsigned long long)p.vertex_cache.hits,(unsigned long long)p.vertex_cache.uploads);
    if(p.frames%300==0)std::fprintf(stderr,"GLES frames=%llu draws=%llu declines=%llu textures=%zu\n",(unsigned long long)p.frames,(unsigned long long)p.draws,(unsigned long long)p.declines,p.textures.size());return glGetError()==GL_NO_ERROR ? RECOMP_D3D_PRESENTER_OK:RECOMP_D3D_PRESENTER_HOST_FAILURE;
}
}
extern "C" RecompD3dPresenterError recomp_d3d_presenter_create(const RecompD3dPresenterConfig *config,RecompD3dPresenter **out)
{
    if(!config || !out || !config->width || !config->height)return RECOMP_D3D_PRESENTER_INVALID_ARGUMENT;if(*out)return RECOMP_D3D_PRESENTER_ALREADY_INITIALIZED;
    auto p=std::make_unique<RecompD3dPresenter>();p->owner=std::this_thread::get_id();
    const char *aspect=std::getenv("RECOMP_DISPLAY_ASPECT");
    if(aspect && !std::strcmp(aspect,"4:3")){p->display_aspect=4.0/3.0;p->display_profile="4:3";}
    else if(aspect && !std::strcmp(aspect,"16:9")){p->display_aspect=16.0/9.0;p->display_profile="16:9";}
    const char *resolution=std::getenv("RECOMP_RENDER_SCALE");p->render_scale=resolution&&!std::strcmp(resolution,"3") ? 3:resolution&&!std::strcmp(resolution,"2") ? 2:1;
    if(!SDL_Init(SDL_INIT_VIDEO))return RECOMP_D3D_PRESENTER_HOST_FAILURE;
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_ES);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,3);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,0);SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER,1);
    p->window=SDL_CreateWindow("Dead or Alive Xtreme Beach Volleyball",config->width,config->height,SDL_WINDOW_OPENGL|SDL_WINDOW_FULLSCREEN|SDL_WINDOW_HIGH_PIXEL_DENSITY);
    if(!p->window)return RECOMP_D3D_PRESENTER_HOST_FAILURE;p->context=SDL_GL_CreateContext(p->window);if(!p->context){SDL_DestroyWindow(p->window);return RECOMP_D3D_PRESENTER_HOST_FAILURE;}SDL_GL_SetSwapInterval(immediate ? 0:1);
    std::fprintf(stderr,"GLES renderer=%s version=%s\n",glGetString(GL_RENDERER),glGetString(GL_VERSION));
    p->adreno=std::strstr((const char *)glGetString(GL_RENDERER),"Adreno")!=nullptr;
    std::fprintf(stderr,"Android GPU profile=%s\n",p->adreno ? "Adreno immutable geometry/orphaned uniforms":"compatible legacy streaming");
    std::fprintf(stderr,"Android display aspect=%s guest=%ux%u\n",p->display_profile,config->width,config->height);
    GLint extensions=0;glGetIntegerv(GL_NUM_EXTENSIONS,&extensions);for(GLint i=0;i<extensions;++i){const char *e=(const char *)glGetStringi(GL_EXTENSIONS,i);if(!std::strcmp(e,"GL_EXT_texture_border_clamp") || !std::strcmp(e,"GL_OES_texture_border_clamp"))p->border=true;}
    glGenVertexArrays(1,&p->vao);glBindVertexArray(p->vao);glGenBuffers(1,&p->vertices);glGenBuffers(1,&p->indices);glGenBuffers(1,&p->constants);glBindBuffer(GL_UNIFORM_BUFFER,p->constants);glBufferData(GL_UNIFORM_BUFFER,1048*4,nullptr,GL_STREAM_DRAW);
    const char *vs="#version 300 es\nconst vec2 points[3]=vec2[3](vec2(-1,-1),vec2(3,-1),vec2(-1,3));out vec2 uv;void main(){vec2 p=points[gl_VertexID];gl_Position=vec4(p,0,1);uv=vec2((p.x+1.)*.5,(1.-p.y)*.5);}";
    const char *ps="#version 300 es\nprecision highp float;uniform sampler2D frame;uniform sampler2D ramp;in vec2 uv;out vec4 result;void main(){vec4 c=texture(frame,uv);result=vec4(texture(ramp,vec2((floor(clamp(c.r,0.,1.)*255.+.5)+.5)/256.,.5)).r,texture(ramp,vec2((floor(clamp(c.g,0.,1.)*255.+.5)+.5)/256.,.5)).g,texture(ramp,vec2((floor(clamp(c.b,0.,1.)*255.+.5)+.5)/256.,.5)).b,c.a);}";
    p->blit=link(vs,ps);glUseProgram(p->blit);glUniform1i(glGetUniformLocation(p->blit,"frame"),0);glUniform1i(glGetUniformLocation(p->blit,"ramp"),1);
    glGenTextures(1,&p->gamma);glBindTexture(GL_TEXTURE_2D,p->gamma);uint8_t ramp[256*4];for(unsigned i=0;i<256;++i){ramp[i*4]=ramp[i*4+1]=ramp[i*4+2]=i;ramp[i*4+3]=255;}glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,256,1,0,GL_RGBA,GL_UNSIGNED_BYTE,ramp);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
    glGenTextures(1,&p->white);glBindTexture(GL_TEXTURE_2D,p->white);uint32_t white=0xffffffff;glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,1,1,0,GL_RGBA,GL_UNSIGNED_BYTE,&white);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    if(!p->blit || !surface(p->main,config->width,config->height,p->render_scale) || glGetError()!=GL_NO_ERROR){*out=p.release();recomp_d3d_presenter_destroy(out);return RECOMP_D3D_PRESENTER_HOST_FAILURE;}
    std::fprintf(stderr,"Android internal resolution=%ux%u scale=%u guest=%ux%u\n",p->main.pixel_width,p->main.pixel_height,p->render_scale,p->main.width,p->main.height);
    *out=p.release();return RECOMP_D3D_PRESENTER_OK;
}
extern "C" RecompD3dPresenterError recomp_d3d_presenter_submit(RecompD3dPresenter *p,const RecompD3dPresenterCommand *cmd)
{
    if(!p)return RECOMP_D3D_PRESENTER_NOT_INITIALIZED;if(p->owner!=std::this_thread::get_id())return RECOMP_D3D_PRESENTER_WRONG_THREAD;if(!cmd)return RECOMP_D3D_PRESENTER_INVALID_ARGUMENT;
    try{switch(cmd->type){case RECOMP_D3D_PRESENTER_COMMAND_DRAW:{auto result=draw(*p,cmd->data.draw);if(result!=RECOMP_D3D_PRESENTER_OK)++p->declines;return result;}
        case RECOMP_D3D_PRESENTER_COMMAND_CLEAR:{const auto &c=cmd->data.clear;p->depth_valid=p->blend_valid=false;if(!target(*p,c.target))return RECOMP_D3D_PRESENTER_UNSUPPORTED_COMMAND;GLbitfield bits=0;if(c.clear_color){float rgba[4];color(c.color,rgba);glColorMask(1,1,1,1);glClearColor(rgba[0],rgba[1],rgba[2],rgba[3]);bits|=GL_COLOR_BUFFER_BIT;}if(c.clear_depth){glDepthMask(1);glClearDepthf(c.z);bits|=GL_DEPTH_BUFFER_BIT;}if(c.clear_stencil){glStencilMask(255);glClearStencil(c.stencil);bits|=GL_STENCIL_BUFFER_BIT;}glClear(bits);return glGetError()==GL_NO_ERROR ? RECOMP_D3D_PRESENTER_OK:RECOMP_D3D_PRESENTER_HOST_FAILURE;}
        case RECOMP_D3D_PRESENTER_COMMAND_PRESENT:return present(*p);
        case RECOMP_D3D_PRESENTER_COMMAND_GAMMA:{uint8_t rgba[256*4];for(unsigned i=0;i<256;++i){for(unsigned c=0;c<3;++c)rgba[i*4+c]=cmd->data.gamma[c][i];rgba[i*4+3]=255;}glActiveTexture(GL_TEXTURE1);glBindTexture(GL_TEXTURE_2D,p->gamma);glTexSubImage2D(GL_TEXTURE_2D,0,0,0,256,1,GL_RGBA,GL_UNSIGNED_BYTE,rgba);return RECOMP_D3D_PRESENTER_OK;}}
    }catch(const std::bad_alloc &){return RECOMP_D3D_PRESENTER_OUT_OF_MEMORY;}catch(const std::exception &e){std::fprintf(stderr,"GLES presenter: %s\n",e.what());return RECOMP_D3D_PRESENTER_HOST_FAILURE;}return RECOMP_D3D_PRESENTER_UNSUPPORTED_COMMAND;
}
extern "C" RecompD3dPresenterError recomp_d3d_presenter_release_memory(RecompD3dPresenter *p,uint32_t base,uint32_t size)
{
    if(!p)return RECOMP_D3D_PRESENTER_NOT_INITIALIZED;if(p->owner!=std::this_thread::get_id())return RECOMP_D3D_PRESENTER_WRONG_THREAD;
    base&=0x7fffffff;auto overlaps=[&](uint32_t address,size_t bytes){address&=0x7fffffff;return uint64_t(address)<uint64_t(base)+size && uint64_t(base)<uint64_t(address)+bytes;};
    for(auto i=p->targets.begin();i!=p->targets.end();){size_t bytes=size_t(i->second.width)*i->second.height*4;if(overlaps(i->first,bytes)){p->target_bytes-=size_t(i->second.pixel_width)*i->second.pixel_height*4;destroy(i->second);i=p->targets.erase(i);}else ++i;}
    for(auto i=p->depths.begin();i!=p->depths.end();){if(i->first && overlaps(i->first,size_t(i->second.width)*i->second.height*4)){glDeleteRenderbuffers(1,&i->second.buffer);i=p->depths.erase(i);}else ++i;}
    for(auto i=p->textures.begin();i!=p->textures.end();){uint32_t address=(uint32_t)std::stoul(i->first);if(overlaps(address,i->second.bytes)){p->texture_bytes-=i->second.bytes;glDeleteTextures(1,&i->second.name);i=p->textures.erase(i);}else ++i;}p->current=nullptr;p->target_valid=false;return RECOMP_D3D_PRESENTER_OK;
}
extern "C" RecompD3dPresenterError recomp_d3d_presenter_destroy(RecompD3dPresenter **out)
{
    if(!out)return RECOMP_D3D_PRESENTER_INVALID_ARGUMENT;if(!*out)return RECOMP_D3D_PRESENTER_NOT_INITIALIZED;auto *p=*out;if(p->owner!=std::this_thread::get_id())return RECOMP_D3D_PRESENTER_WRONG_THREAD;
    destroy(p->main);destroy(p->feedback);for(auto &t:p->targets)destroy(t.second);for(auto &t:p->textures)glDeleteTextures(1,&t.second.name);for(auto &d:p->depths)glDeleteRenderbuffers(1,&d.second.buffer);for(auto &s:p->pipelines)glDeleteProgram(s.second.name);
    p->index_cache.clear();p->vertex_cache.clear();glDeleteProgram(p->blit);glDeleteBuffers(1,&p->vertices);glDeleteBuffers(1,&p->indices);glDeleteBuffers(1,&p->constants);glDeleteTextures(1,&p->gamma);glDeleteTextures(1,&p->white);glDeleteVertexArrays(1,&p->vao);SDL_GL_DestroyContext(p->context);SDL_DestroyWindow(p->window);delete p;*out=nullptr;return RECOMP_D3D_PRESENTER_OK;
}
extern "C" void recomp_d3d_presenter_set_immediate_present(bool enabled){immediate=enabled;}
extern "C" float recomp_android_presenter_fps(void){return SDL_GetTicks()-fps_updated_ms.load(std::memory_order_relaxed)>2500 ? 0.f:visible_fps.load(std::memory_order_relaxed);}
extern "C" void recomp_d3d_presenter_report_draw_textures(void){std::fprintf(stderr,"Android GLES texture counters are reported per presenter frame batch\n");}

extern "C" void recomp_android_display_settings(unsigned scale,unsigned aspect){if(scale>=1&&scale<=3&&aspect<=2)requested_display.store(scale|(aspect<<2),std::memory_order_release);}

extern "C" unsigned recomp_android_consume_display_failure(void){return display_failure.exchange(0,std::memory_order_acq_rel);}

extern "C" uint64_t recomp_android_presented_frames(void){return presented_frames.load(std::memory_order_acquire);}
