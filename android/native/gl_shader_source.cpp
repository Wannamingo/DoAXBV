#include "gl_shader_source.h"
#include <glslang/Public/ResourceLimits.h>
#include <glslang/Public/ShaderLang.h>
#include <SPIRV/GlslangToSpv.h>
#include <spirv_glsl.hpp>
#include <mutex>
bool recomp_android_draw_hlsl(const RecompD3dPresenterDrawCommand &,std::string &);
namespace {
std::once_flag init;
std::string convert(glslang::TIntermediate *intermediate,bool vertex,RecompGlShaderSource &out)
{
    std::vector<uint32_t> words;
    glslang::SpvOptions spv_options; spv_options.disableOptimizer=true;
    glslang::GlslangToSpv(*intermediate,words,&spv_options);
    spirv_cross::CompilerGLSL compiler(std::move(words));
    auto resources=compiler.get_shader_resources();
    // Mali r44p1 accepts different VS/PS instance names but then reads stale
    // fragment constants from a previously used buffer. Give the shared block
    // the same instance name in both stages. Alternating-buffer GPU readbacks
    // verify texture/alpha parameters, not just the reported std140 offsets.
    for(const auto &buffer:resources.uniform_buffers)
        compiler.set_name(buffer.id,"guest_draw_constants");
    // GLES 3.0 links inter-stage variables by name. HLSL uses different
    // "input" and "output" names; glslang's linked locations are authoritative.
    for(const auto &varying:vertex ? resources.stage_outputs : resources.stage_inputs) {
        unsigned location=compiler.get_decoration(varying.id,spv::DecorationLocation);
        compiler.set_name(varying.id,"guest_varying_"+std::to_string(location));
    }
    if(vertex) for(const auto &input:resources.stage_inputs)
        out.attributes.push_back({input.name,compiler.get_decoration(input.id,spv::DecorationLocation)});
    compiler.build_combined_image_samplers();
    for(const auto &combined:compiler.get_combined_image_samplers()) {
        std::string image=compiler.get_name(combined.image_id),sampler=compiler.get_name(combined.sampler_id);
        std::string name=image+"_"+sampler;
        compiler.set_name(combined.combined_id,name);
        out.samplers.push_back({name,image=="alpha_mask" ? 1u : 0u});
    }
    spirv_cross::CompilerGLSL::Options options;
    options.version=300;options.es=true;options.enable_420pack_extension=false;
    options.vertex.fixup_clipspace=true;options.vertex.flip_vert_y=true;
    options.fragment.default_float_precision=spirv_cross::CompilerGLSL::Options::Highp;
    options.fragment.default_int_precision=spirv_cross::CompilerGLSL::Options::Highp;
    compiler.set_common_options(options);
    return compiler.compile();
}
}
bool recomp_gl_shader_source(const RecompD3dPresenterDrawCommand &draw,RecompGlShaderSource &out,std::string &error)
{
    out={};error.clear();
    std::call_once(init,[]{glslang::InitializeProcess();});
    std::string hlsl;
    if(!recomp_android_draw_hlsl(draw,hlsl)){error="Unsupported FVF or vertex program";return false;}
    const char *source=hlsl.c_str();
    glslang::TShader vertex(EShLangVertex),fragment(EShLangFragment);
    for(auto *shader:{&vertex,&fragment}) {
        shader->setStrings(&source,1);
        const char *entry=shader==&vertex ? "vs_main" : "ps_main";
        shader->setEntryPoint(entry);shader->setSourceEntryPoint(entry);
        shader->setEnvInput(glslang::EShSourceHlsl,shader==&vertex ? EShLangVertex : EShLangFragment,glslang::EShClientVulkan,100);
        shader->setEnvClient(glslang::EShClientVulkan,glslang::EShTargetVulkan_1_0);
        shader->setEnvTarget(glslang::EShTargetSpv,glslang::EShTargetSpv_1_0);
        shader->setAutoMapBindings(true);shader->setAutoMapLocations(true);
        shader->setShiftSamplerBinding(32);
        auto messages=(EShMessages)(EShMsgReadHlsl|EShMsgSpvRules|EShMsgVulkanRules);
        if(!shader->parse(GetDefaultResources(),100,false,messages)){error=shader->getInfoLog();return false;}
    }
    glslang::TProgram program;program.addShader(&vertex);program.addShader(&fragment);
    if(!program.link((EShMessages)(EShMsgSpvRules|EShMsgVulkanRules)) || !program.mapIO()){
        error=program.getInfoLog();return false;
    }
    try {
        out.vertex=convert(program.getIntermediate(EShLangVertex),true,out);
        out.fragment=convert(program.getIntermediate(EShLangFragment),false,out);
        return true;
    } catch(const std::exception &e){error=e.what();return false;}
}
