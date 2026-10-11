// SPDX-License-Identifier: GPL-2.0-or-later
#include "gl_shader_source.h"
#include "d3d_draw_model.h"
#include "gles_buffer_cache.h"
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <cstdio>
#include <cstring>
#include <vector>
static GLuint stage(GLenum type,const std::string &source){GLuint s=glCreateShader(type);const char *text=source.c_str();glShaderSource(s,1,&text,nullptr);glCompileShader(s);GLint okay=0;glGetShaderiv(s,GL_COMPILE_STATUS,&okay);if(!okay){char log[8192];glGetShaderInfoLog(s,sizeof log,nullptr,log);std::fprintf(stderr,"Shader compile: %s\n%s\n",log,source.c_str());glDeleteShader(s);return 0;}return s;}
static bool screen_pipeline(unsigned fvf,int alpha_function=-1,unsigned reference=128,bool orphaned=false)
{
    RecompD3dPresenterDrawCommand command{};command.fvf=fvf;command.depth.alpha_test_enable=alpha_function>=0;command.depth.alpha_func=(RecompD3dCompareFunc)(alpha_function<0 ? 0:alpha_function);
    RecompGlShaderSource source;std::string error;RecompD3dVertexLayout layout{};
    if(!recomp_d3d_fvf_layout(fvf,&layout)||!recomp_gl_shader_source(command,source,error))return false;
    GLuint vs=stage(GL_VERTEX_SHADER,source.vertex),fs=stage(GL_FRAGMENT_SHADER,source.fragment),program=glCreateProgram();glAttachShader(program,vs);glAttachShader(program,fs);glLinkProgram(program);GLint linked=0;glGetProgramiv(program,GL_LINK_STATUS,&linked);if(!linked)return false;glUseProgram(program);
    std::vector<uint8_t> vertices(layout.stride*3);
    float positions[3][4]={{-.75f,-.75f,.5f,1},{.75f,-.75f,.5f,1},{0,.75f,.5f,1}};
    for(unsigned i=0;i<3;++i){auto *v=vertices.data()+i*layout.stride;std::memcpy(v,positions[i],16);if(layout.diffuse_offset!=RECOMP_D3D_FVF_ABSENT)std::memset(v+layout.diffuse_offset,255,4);float uv[2]={.25f,.25f};std::memcpy(v+layout.texcoord_offset,uv,8);}
    GLuint vao,vbo,ubo,texture,mask;glGenVertexArrays(1,&vao);glBindVertexArray(vao);glGenBuffers(1,&vbo);glBindBuffer(GL_ARRAY_BUFFER,vbo);glBufferData(GL_ARRAY_BUFFER,vertices.size(),vertices.data(),GL_STATIC_DRAW);
    for(auto &input:source.attributes){unsigned offset=0,components=4;GLenum type=GL_FLOAT;GLboolean normalized=0;if(input.name.find("diffuse")!=std::string::npos){offset=layout.diffuse_offset;type=GL_UNSIGNED_BYTE;normalized=1;}else if(input.name.find("texcoord")!=std::string::npos){offset=layout.texcoord_offset;components=2;}glEnableVertexAttribArray(input.location);glVertexAttribPointer(input.location,components,type,normalized,layout.stride,(const void *)(uintptr_t)offset);std::printf("Screen fvf=%x attribute=%s location=%u offset=%u\n",fvf,input.name.c_str(),input.location,offset);}
    float constants[1048]{};constants[0]=constants[5]=constants[10]=constants[15]=1;constants[64]=constants[70]=constants[80]=constants[81]=1;constants[65]=alpha_function>=0;constants[66]=float(alpha_function);constants[67]=float(reference);
    glGenBuffers(1,&ubo);glBindBuffer(GL_UNIFORM_BUFFER,ubo);
    if(orphaned){float stale[1048]{};stale[67]=203;glBufferData(GL_UNIFORM_BUFFER,sizeof stale,stale,GL_STREAM_DRAW);}
    glBufferData(GL_UNIFORM_BUFFER,sizeof constants,constants,orphaned ? GL_STREAM_DRAW:GL_STATIC_DRAW);glUniformBlockBinding(program,glGetUniformBlockIndex(program,"Transform"),0);glBindBufferBase(GL_UNIFORM_BUFFER,0,ubo);
    const uint8_t rgba[4]={40,160,80,128},white[4]={255,255,255,255};
    for(unsigned unit=0;unit<2;++unit){GLuint name;glGenTextures(1,&name);if(unit)mask=name;else texture=name;glActiveTexture(GL_TEXTURE0+unit);glBindTexture(GL_TEXTURE_2D,name);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,1,1,0,GL_RGBA,GL_UNSIGNED_BYTE,unit ? white:rgba);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);}
    for(auto &sampler:source.samplers){GLint location=glGetUniformLocation(program,sampler.first.c_str());std::printf("Screen sampler %s unit=%u location=%d\n",sampler.first.c_str(),sampler.second,location);if(location>=0)glUniform1i(location,sampler.second);}
    glDisable(GL_BLEND);glDisable(GL_DEPTH_TEST);glDisable(GL_CULL_FACE);glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT);glDrawArrays(GL_TRIANGLES,0,3);uint8_t pixel[4];glReadPixels(8,8,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixel);
    const bool pass=alpha_function<0 || alpha_function==7 ||
        (alpha_function==1&&128<reference) || (alpha_function==2&&128==reference) ||
        (alpha_function==3&&128<=reference) || (alpha_function==4&&128>reference) ||
        (alpha_function==5&&128!=reference) || (alpha_function==6&&128>=reference);
    const uint8_t clear[4]={0,0,0,255};const uint8_t *expected=pass ? rgba:clear;
    GLenum gl_error=glGetError();bool okay=!std::memcmp(pixel,expected,4)&&gl_error==GL_NO_ERROR;
    std::printf("Screen fvf=%x alpha=%d ref=%u readback=%u,%u,%u,%u pass=%u GLerror=%x\n",fvf,alpha_function,reference,pixel[0],pixel[1],pixel[2],pixel[3],unsigned(okay),gl_error);
    if(!okay)std::fprintf(stderr,"%s\n%s\n",source.vertex.c_str(),source.fragment.c_str());
    glDeleteTextures(1,&texture);glDeleteTextures(1,&mask);glDeleteBuffers(1,&vbo);glDeleteBuffers(1,&ubo);glDeleteVertexArrays(1,&vao);glDeleteProgram(program);glDeleteShader(vs);glDeleteShader(fs);return okay;
}
int main()
{
    EGLDisplay display=eglGetDisplay(EGL_DEFAULT_DISPLAY);if(!eglInitialize(display,nullptr,nullptr)){std::fprintf(stderr,"EGL init %x\n",eglGetError());return 1;}
    EGLint attributes[]={EGL_SURFACE_TYPE,EGL_PBUFFER_BIT,EGL_RENDERABLE_TYPE,0x40,EGL_RED_SIZE,8,EGL_GREEN_SIZE,8,EGL_BLUE_SIZE,8,EGL_ALPHA_SIZE,8,EGL_DEPTH_SIZE,24,EGL_NONE};EGLConfig config;EGLint count=0;
    if(!eglChooseConfig(display,attributes,&config,1,&count)||!count)return 2;EGLint pixels[]={EGL_WIDTH,16,EGL_HEIGHT,16,EGL_NONE},version[]={EGL_CONTEXT_CLIENT_VERSION,3,EGL_NONE};EGLSurface surface=eglCreatePbufferSurface(display,config,pixels);EGLContext context=eglCreateContext(display,config,EGL_NO_CONTEXT,version);if(!eglMakeCurrent(display,surface,surface,context))return 3;
    std::printf("GPU: %s; %s\n",glGetString(GL_RENDERER),glGetString(GL_VERSION));GLuint program=0;std::vector<std::pair<std::string,unsigned>> samplers;
    const unsigned layouts[]={0x002,0x012,0x042,0x102,0x112,0x142,0x144,0x116,0x118,0x11a,0x212,0x242,0x244,0x412,0x442,0x444};
    for(unsigned fvf:layouts){RecompD3dPresenterDrawCommand d{};d.fvf=fvf;RecompGlShaderSource source;std::string error;if(!recomp_gl_shader_source(d,source,error)){std::fprintf(stderr,"Translation: %s\n",error.c_str());return 4;}
        GLuint vs=stage(GL_VERTEX_SHADER,source.vertex),fs=stage(GL_FRAGMENT_SHADER,source.fragment);if(!vs||!fs)return 5;GLuint p=glCreateProgram();glAttachShader(p,vs);glAttachShader(p,fs);glLinkProgram(p);glDeleteShader(vs);glDeleteShader(fs);GLint okay=0;glGetProgramiv(p,GL_LINK_STATUS,&okay);if(!okay){char log[8192];glGetProgramInfoLog(p,sizeof log,nullptr,log);std::fprintf(stderr,"fvf=%x link: %s\n",fvf,log);return 6;}
        GLuint block=glGetUniformBlockIndex(p,"Transform");GLint size=0;if(block!=GL_INVALID_INDEX)glGetActiveUniformBlockiv(p,block,GL_UNIFORM_BLOCK_DATA_SIZE,&size);if(size!=4192){std::fprintf(stderr,"fvf=%x constant layout %d\n",fvf,size);return 7;}std::printf("fvf=%x GPU compile/link and constant layout passed\n",fvf);if(fvf==0x42){program=p;samplers=source.samplers;}else glDeleteProgram(p);}
    struct Vertex{float x,y,z;uint8_t rgba[4];};const Vertex vertices[]={{-.75f,-.75f,.5f,{255,0,0,255}},{.75f,-.75f,.5f,{255,0,0,255}},{0,.75f,.5f,{255,0,0,255}}};
    GLuint vao,vbo,ubo;glGenVertexArrays(1,&vao);glBindVertexArray(vao);glGenBuffers(1,&vbo);glBindBuffer(GL_ARRAY_BUFFER,vbo);glBufferData(GL_ARRAY_BUFFER,sizeof vertices,vertices,GL_STATIC_DRAW);glEnableVertexAttribArray(0);glVertexAttribPointer(0,3,GL_FLOAT,0,sizeof(Vertex),nullptr);glEnableVertexAttribArray(1);glVertexAttribPointer(1,4,GL_UNSIGNED_BYTE,1,sizeof(Vertex),(const void *)12);
    float constants[1048]{};constants[0]=constants[5]=constants[10]=constants[15]=1;constants[70]=1;constants[67]=203;glGenBuffers(1,&ubo);glBindBuffer(GL_UNIFORM_BUFFER,ubo);glBufferData(GL_UNIFORM_BUFFER,sizeof constants,constants,GL_DYNAMIC_DRAW);glUniformBlockBinding(program,glGetUniformBlockIndex(program,"Transform"),0);glBindBufferBase(GL_UNIFORM_BUFFER,0,ubo);
    glUseProgram(program);GLuint dummy;glGenTextures(1,&dummy);glBindTexture(GL_TEXTURE_2D,dummy);uint32_t white=0xffffffff;glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,1,1,0,GL_RGBA,GL_UNSIGNED_BYTE,&white);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    for(auto &sampler:samplers){GLint location=glGetUniformLocation(program,sampler.first.c_str());std::printf("Sampler %s unit=%u location=%d\n",sampler.first.c_str(),sampler.second,location);if(location>=0)glUniform1i(location,sampler.second);}
    glViewport(0,0,16,16);glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);glDrawArrays(GL_TRIANGLES,0,3);uint8_t pixel[4];glReadPixels(8,8,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixel);if(pixel[0]!=255||pixel[1]||pixel[2]||pixel[3]!=255||glGetError()!=GL_NO_ERROR){std::fprintf(stderr,"RGBA readback %u %u %u %u\n",pixel[0],pixel[1],pixel[2],pixel[3]);return 8;}
    // Exercise the runtime's actual named sampler mapping, not just untextured
    // geometry. White fallback textures must not silently hide a bad binding.
    // The guest triangle is CCW. clip-Y conversion makes it CW in GLES;
    // culling guest CW faces must retain it, and the opposite must remove it.
    glEnable(GL_CULL_FACE);glCullFace(GL_BACK);glFrontFace(GL_CW);glClear(GL_COLOR_BUFFER_BIT);glDrawArrays(GL_TRIANGLES,0,3);glReadPixels(8,8,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixel);if(pixel[0]!=255||pixel[1]||pixel[2])return 13;
    glFrontFace(GL_CCW);glClearColor(0,0,1,1);glClear(GL_COLOR_BUFFER_BIT);glDrawArrays(GL_TRIANGLES,0,3);glReadPixels(8,8,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixel);if(pixel[0]||pixel[1]||pixel[2]!=255)return 14;glDisable(GL_CULL_FACE);
    constants[64]=1;constants[80]=constants[81]=1;glBufferSubData(GL_UNIFORM_BUFFER,0,sizeof constants,constants);uint8_t texel[4]={128,0,0,255};glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,dummy);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,1,1,0,GL_RGBA,GL_UNSIGNED_BYTE,texel);glActiveTexture(GL_TEXTURE1);glBindTexture(GL_TEXTURE_2D,dummy);glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT);glDrawArrays(GL_TRIANGLES,0,3);glReadPixels(8,8,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixel);if(pixel[0]!=128||pixel[1]||pixel[2]||pixel[3]!=255){std::fprintf(stderr,"Texture readback %u %u %u %u\n",pixel[0],pixel[1],pixel[2],pixel[3]);return 10;}
    std::puts("GLES: triangle and named texture sampling readbacks passed.");
    // Verify immutable-index reuse by content, mutations at the same pointer,
    // a different pointer with identical bytes, and repeated bounded eviction.
    // Live GL readbacks ensure deletion cannot change already submitted draws.
    RecompGlesBufferCache index_cache(24,2);
    uint16_t index_data[3]={0,1,2},same_content[3]={0,1,2};
    GLuint first=index_cache.get(index_data,sizeof index_data);
    if(!first || index_cache.get(same_content,sizeof same_content)!=first || index_cache.hits!=1)return 15;
    for(unsigned i=0;i<18;++i){
        unsigned mode=i%3;index_data[0]=0;index_data[1]=mode==1 ? 0:mode==2 ? 2:1;index_data[2]=mode==1 ? 0:mode==2 ? 1:2;
        GLuint buffer=index_cache.get(index_data,sizeof index_data);if(!buffer)return 16;
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,buffer);glClear(GL_COLOR_BUFFER_BIT);glDrawElements(GL_TRIANGLES,3,GL_UNSIGNED_SHORT,nullptr);glReadPixels(8,8,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixel);
        if(pixel[0]!=(mode==1 ? 0:128)||pixel[1]||pixel[2]||pixel[3]!=255||glGetError()!=GL_NO_ERROR)return 17;
    }
    if(index_cache.uploads<10 || index_cache.get(index_data,25))return 18;
    RecompGlesBufferCache vertex_cache(sizeof vertices*2,2,GL_ARRAY_BUFFER);
    Vertex changed[3];std::memcpy(changed,vertices,sizeof vertices);GLuint original_vbo=vertex_cache.get(changed,sizeof changed);
    if(!original_vbo || vertex_cache.get(vertices,sizeof vertices)!=original_vbo || vertex_cache.hits!=1)return 21;
    for(unsigned i=0;i<12;++i){
        std::memcpy(changed,vertices,sizeof vertices);if(i%3==1)for(auto &v:changed)v.x+=4;else if(i%3==2)for(auto &v:changed)v.z=.25f;
        GLuint buffer=vertex_cache.get(changed,sizeof changed);if(!buffer)return 22;glBindBuffer(GL_ARRAY_BUFFER,buffer);
        glVertexAttribPointer(0,3,GL_FLOAT,0,sizeof(Vertex),nullptr);glVertexAttribPointer(1,4,GL_UNSIGNED_BYTE,1,sizeof(Vertex),(const void *)12);
        glClear(GL_COLOR_BUFFER_BIT);glDrawArrays(GL_TRIANGLES,0,3);glReadPixels(8,8,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixel);
        if(pixel[0]!=(i%3==1 ? 0:128)||pixel[1]||pixel[2]||pixel[3]!=255||glGetError()!=GL_NO_ERROR)return 23;
    }
    vertex_cache.clear();std::puts("GLES: vertex content reuse, same-pointer mutation and eviction readbacks passed.");
    index_cache.clear();std::puts("GLES: index content reuse, mutation, bound and eviction readbacks passed.");glDeleteProgram(program);
    for(unsigned fvf:{0x104u,0x144u}) {
        if(!screen_pipeline(fvf))return 11;
        for(int func=0;func<8;++func)for(unsigned ref:{127u,128u,129u})
            if(!screen_pipeline(fvf,func,ref))return 12;
    }
    std::puts("GLES: video/UI sampling and all eight 8-bit alpha comparisons passed.");
    for(unsigned fvf:{0x104u,0x144u}) {
        if(!screen_pipeline(fvf,-1,128,true))return 19;
        for(int func=0;func<8;++func)for(unsigned ref:{127u,128u,129u})
            if(!screen_pipeline(fvf,func,ref,true))return 20;
    }
    std::puts("GLES: orphaned shared VS/FS constants preserve video/UI and alpha readbacks.");
    eglMakeCurrent(display,EGL_NO_SURFACE,EGL_NO_SURFACE,EGL_NO_CONTEXT);eglDestroyContext(display,context);eglDestroySurface(display,surface);eglTerminate(display);return 0;
}
