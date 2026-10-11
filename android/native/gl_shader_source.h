#ifndef DOAXBV_GL_SHADER_SOURCE_H
#define DOAXBV_GL_SHADER_SOURCE_H
#include "d3d_presenter.h"
#include <string>
#include <vector>
struct RecompGlAttribute { std::string name; unsigned location; };
struct RecompGlShaderSource {
    std::string vertex, fragment;
    std::vector<RecompGlAttribute> attributes;
    std::vector<std::pair<std::string,unsigned>> samplers;
};
bool recomp_gl_shader_source(const RecompD3dPresenterDrawCommand &,RecompGlShaderSource &,std::string &);
#endif
