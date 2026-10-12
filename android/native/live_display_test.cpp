// SPDX-License-Identifier: GPL-2.0-or-later
// Exercise the presenter's private resize path against real GLES allocations.
// Unused SDL/guest entry points are discarded from this standalone GPU test.
#include "d3d_presenter_gles.cpp"
#include <EGL/egl.h>
#define CHECK(c) do { if(!(c)){std::fprintf(stderr,"Live display check failed at line %d: %s\n",__LINE__,#c);return 1;} } while(0)
static bool pixel(const Surface &s,unsigned r,unsigned g,unsigned b){
    glBindFramebuffer(GL_FRAMEBUFFER,s.fbo);unsigned char rgba[4]{};
    glReadPixels(s.pixel_width/2,s.pixel_height/2,1,1,GL_RGBA,GL_UNSIGNED_BYTE,rgba);
    return rgba[0]==r&&rgba[1]==g&&rgba[2]==b&&rgba[3]==255&&glGetError()==GL_NO_ERROR;
}
int main(){
    EGLDisplay display=eglGetDisplay(EGL_DEFAULT_DISPLAY);CHECK(eglInitialize(display,nullptr,nullptr));
    const EGLint attributes[]={EGL_SURFACE_TYPE,EGL_PBUFFER_BIT,EGL_RENDERABLE_TYPE,EGL_OPENGL_ES3_BIT,EGL_RED_SIZE,8,EGL_GREEN_SIZE,8,EGL_BLUE_SIZE,8,EGL_ALPHA_SIZE,8,EGL_NONE};
    EGLConfig config;EGLint count;CHECK(eglChooseConfig(display,attributes,&config,1,&count)&&count);
    const EGLint context_attributes[]={EGL_CONTEXT_CLIENT_VERSION,3,EGL_NONE};
    EGLContext context=eglCreateContext(display,config,EGL_NO_CONTEXT,context_attributes);CHECK(context!=EGL_NO_CONTEXT);
    const EGLint surface_attributes[]={EGL_WIDTH,16,EGL_HEIGHT,16,EGL_NONE};
    EGLSurface window=eglCreatePbufferSurface(display,config,surface_attributes);CHECK(window!=EGL_NO_SURFACE);CHECK(eglMakeCurrent(display,window,window,context));
    RecompD3dPresenter p;CHECK(surface(p.main,720,480,1));glClearColor(1,0,0,1);glClear(GL_COLOR_BUFFER_BIT);
    Surface offscreen;CHECK(surface(offscreen,64,32,1));glClearColor(0,0,1,1);glClear(GL_COLOR_BUFFER_BIT);p.targets.emplace(4096,offscreen);p.target_bytes=64*32*4;
    CHECK(surface(p.feedback,64,32,1));Depth depth;glGenRenderbuffers(1,&depth.buffer);p.depths.emplace(0,depth);p.current=&p.main;
    for(unsigned scale:{3u,1u,2u,3u,1u}){
        CHECK(resize_surfaces(p,scale));CHECK(p.render_scale==scale&&p.main.pixel_width==720*scale&&p.main.pixel_height==480*scale);
        CHECK(p.targets.at(4096).pixel_width==64*scale&&p.targets.at(4096).pixel_height==32*scale);
        CHECK(p.target_bytes==64*32*4*scale*scale&&p.current==nullptr&&p.depths.empty()&&!p.feedback.fbo);
        CHECK(pixel(p.main,255,0,0)&&pixel(p.targets.at(4096),0,0,255));
    }
    GLuint previous=p.main.fbo,old_target=p.targets.at(4096).fbo;
    CHECK(!resize_surfaces(p,0));while(glGetError()!=GL_NO_ERROR){}
    CHECK(p.main.fbo==previous&&p.targets.at(4096).fbo==old_target&&p.render_scale==1);
    CHECK(pixel(p.main,255,0,0)&&pixel(p.targets.at(4096),0,0,255));
    destroy(p.main);destroy(p.targets.at(4096));p.targets.clear();
    eglMakeCurrent(display,EGL_NO_SURFACE,EGL_NO_SURFACE,EGL_NO_CONTEXT);eglDestroySurface(display,window);eglDestroyContext(display,context);eglTerminate(display);
    std::puts("Live display: real GPU 1x/2x/3x transitions preserve main/offscreen pixels; failed allocation retains old targets.");return 0;
}
