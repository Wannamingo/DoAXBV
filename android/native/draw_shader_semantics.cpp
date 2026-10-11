// SPDX-License-Identifier: GPL-3.0-or-later
// Shared API-level shader semantics copied from the existing D3D11 renderer.
#include "gl_shader_source.h"
#include "d3d_draw_model.h"
#include "d3d_vertex_program.h"
#include <cstdio>
namespace {
constexpr char kDrawShaderPrologue[] =
    "cbuffer Transform : register(b0) {\n"
    "    row_major float4x4 wvp[4];\n"
    "    float4 draw_flags;\n"
    "    float4 blend_flags;\n"
    "    float4 texture_factor;\n"
    "    float4 lighting_flags;\n"
    "    float4 texture_flags;\n"
    "    row_major float4x4 reflection_world_view;\n"
    "    row_major float4x4 reflection_normal;\n"
    "    row_major float4x4 reflection_transform;\n"
    "    float4 reflection_diffuse;\n"
    "    float4 reflection_flags;\n"
    "    float4 vc[192];\n"
    "    row_major float4x4 directional_normals[4];\n"
    "    float4 directional_base;\n"
    "    float4 directional_material;\n"
    "    float4 directional_directions[8];\n"
    "    float4 directional_colors[8];\n"
    "    float4 directional_flags;\n"
    "}\n"
    "Texture2D guest_texture : register(t0);\n"
    "SamplerState guest_sampler : register(s0);\n"
    "Texture2D alpha_mask : register(t1);\n"
    "SamplerState mask_sampler : register(s1);\n";

/* Builds the draw shader for one decoded vertex layout. Only the components
   the layout actually carries appear in VSIn, so the input layout and the
   shader signature always agree. */
void buildDrawShaderSource(
    const RecompD3dVertexLayout &layout,
    char *out,
    size_t out_size)
{
    const bool has_normal =
        layout.normal_offset != RECOMP_D3D_FVF_ABSENT;
    const bool has_diffuse =
        layout.diffuse_offset != RECOMP_D3D_FVF_ABSENT;
    const bool has_texcoord =
        layout.texcoord_offset != RECOMP_D3D_FVF_ABSENT;
    const bool four_coords = layout.texcoord_count == 4u;
    const char *extra_coords = four_coords
        ? "    float2 texcoord1 : TEXCOORD1;\n"
          "    float2 texcoord2 : TEXCOORD2;\n"
          "    float2 texcoord3 : TEXCOORD3;\n"
        : layout.texcoord_count == 2u ? "    float2 texcoord1 : TEXCOORD1;\n" : "";

    char position[768];
    if (layout.pretransformed) {
        std::snprintf(position, sizeof position,
            "    output.position = mul(float4(input.position.xyz, 1.0f), wvp[0]) / input.position.w;\n");
    } else if (layout.blend_weight_count != 0u) {
        std::snprintf(position, sizeof position,
            "    if (blend_flags.x > 0.5f) {\n"
            "    output.position = float4(0, 0, 0, 0);\n"
            "    float remainder = 1.0f;\n"
            "    [unroll] for (uint i = 0; i < %u; ++i) {\n"
            "        output.position += input.weights[i] * mul(float4(input.position, 1), wvp[i]);\n"
            "        remainder -= input.weights[i];\n"
            "    }\n"
            "    output.position += remainder * mul(float4(input.position, 1), wvp[%u]);\n"
            "    } else { output.position = mul(float4(input.position, 1), wvp[0]); }\n",
            layout.blend_weight_count, layout.blend_weight_count);
    } else {
        std::snprintf(position, sizeof position,
            "    output.position = mul(float4(input.position, 1.0f), wvp[0]);\n");
    }
    std::snprintf(
        out,
        out_size,
        "%s"
        "struct VSIn {\n"
        "    %s position : POSITION;\n"
        "%s%s%s%s%s"
        "};\n"
        "struct VSOut {\n"
        "    float4 position : SV_POSITION;\n"
        "    float4 color : COLOR0;\n"
        "    float2 texcoord : TEXCOORD0;\n"
        "    float2 reflection_coord : TEXCOORD4;\n"
        "%s"
        "};\n"
        "VSOut vs_main(VSIn input) {\n"
        "    VSOut output;\n"
        "%s"
        "%s"
        "%s"
        "%s"
        "    output.reflection_coord = float2(0, 0);\n"
        "%s"
        "    return output;\n"
        "}\n"
        "float4 ps_main(VSOut input) : SV_TARGET {\n"
        "    float4 shaded = input.color;\n"
        "    if (reflection_flags.x > 0.5f) {\n"
        "        float4 base = guest_texture.Sample(guest_sampler, input.texcoord);\n"
        "        base.a *= reflection_diffuse.a;\n"
        "        float4 env = alpha_mask.Sample(guest_sampler, input.reflection_coord);\n"
        "        float4 diffuse = reflection_diffuse;\n"
        "        if (directional_flags.x > 0.5f) diffuse.rgb = input.color.rgb;\n"
        "        shaded = lerp(base, env, env.a) * diffuse;\n"
        "    } else\n"
        "%s"
        "    if (blend_flags.y == 1.0f) {\n"
        "        shaded = texture_factor;\n"
        "    } else if (draw_flags.x > 0.5f) {\n"
        "        float4 sampled = guest_texture.Sample(guest_sampler, input.texcoord * texture_flags.xy);\n"
        "        if (lighting_flags.y > 0.5f) sampled.rgb = 1.0f;\n"
        "        shaded %s sampled;\n"
        "        if (directional_flags.x > 0.5f) shaded.rgb *= input.color.rgb;\n"
        "        if (lighting_flags.x > 0.5f) shaded.rgb = 0.0f;\n"
        "        shaded.a = blend_flags.w > 0.5f\n"
        "            ? blend_flags.z : shaded.a * blend_flags.z;\n"
        "        if (blend_flags.y > 1.5f) shaded *= texture_factor;\n"
        "    }\n"
        "    if (reflection_flags.z > 0.5f) shaded.a *= alpha_mask.SampleBias(mask_sampler, input.reflection_coord, reflection_flags.w).a;\n"
        /* NV2A compares rounded 8-bit alpha. Function values follow
           RecompD3dCompareFunc, from NEVER (0) to ALWAYS (7). */
        "    if (draw_flags.y > 0.5f) {\n"
        "        float alpha = round(saturate(shaded.a) * 255.0f);\n"
        "        float ref = draw_flags.w;\n"
        "        int func = (int)draw_flags.z;\n"
        "        bool alpha_pass = func == 7 ||\n"
        "            (func == 1 && alpha < ref) ||\n"
        "            (func == 2 && alpha == ref) ||\n"
        "            (func == 3 && alpha <= ref) ||\n"
        "            (func == 4 && alpha > ref) ||\n"
        "            (func == 5 && alpha != ref) ||\n"
        "            (func == 6 && alpha >= ref);\n"
        "        if (!alpha_pass) discard;\n"
        "    }\n"
        "    return shaded;\n"
        "}\n",
        kDrawShaderPrologue,
        layout.pretransformed ? "float4" : "float3",
        layout.blend_weight_count ? "    float3 weights : BLENDWEIGHT;\n" : "",
        has_normal ? "    float3 normal : NORMAL;\n" : "",
        has_diffuse ? "    float4 diffuse : COLOR0;\n" : "",
        has_texcoord ? "    float2 texcoord : TEXCOORD0;\n" : "",
        extra_coords,
        extra_coords,
        position,
        /* Guest vertex color when the stream has one; otherwise the normal
           stands in so surfaces remain distinguishable. */
        has_diffuse
            ? "    output.color = input.diffuse;\n"
            : (has_normal
                   ? "    output.color = "
                     "float4(abs(normalize(input.normal)), 1.0f);\n"
                   : "    output.color = float4(0.75f, 0.75f, 0.78f, 1.0f);\n"),
        has_texcoord
            ? "    output.texcoord = input.texcoord;\n"
            : "    output.texcoord = float2(0.0f, 0.0f);\n",
        four_coords
            ? "    output.texcoord1 = input.texcoord1;\n"
              "    output.texcoord2 = input.texcoord2;\n"
              "    output.texcoord3 = input.texcoord3;\n"
            : layout.texcoord_count == 2u ? "    output.texcoord1 = input.texcoord1;\n" : "",
        has_texcoord && !layout.pretransformed
            ? (has_normal
                ? "    if (reflection_flags.x > 1.5f) {\n"
                  "        output.reflection_coord = input.texcoord;\n"
                  "    } else if (reflection_flags.x > 0.5f) {\n"
                  "        float3 eye = mul(float4(input.position, 1), reflection_world_view).xyz;\n"
                  "        float3 n = mul(float4(input.normal, 0), reflection_normal).xyz;\n"
                  "        if (reflection_flags.y > 0.5f) n = normalize(n);\n"
                  "        float3 r = reflect(normalize(eye), n);\n"
                  "        output.reflection_coord = mul(float4(r, 1), reflection_transform).xy;\n"
                  "    }\n"
                : "    if (reflection_flags.x > 1.5f) {\n"
                  "        output.reflection_coord = input.texcoord;\n"
                  "    } else if (reflection_flags.x > 0.5f) {\n"
                  "        float3 eye = mul(float4(input.position, 1), reflection_world_view).xyz;\n"
                  "        float3 r = normalize(eye);\n"
                  "        output.reflection_coord = mul(float4(r, 1), reflection_transform).xy;\n"
                  "    }\n")
            : "",
        four_coords
            ? "    if (texture_flags.z > 0.5f) {\n"
              "        float4 t0 = guest_texture.Sample(guest_sampler, input.texcoord * texture_flags.xy);\n"
              "        float4 t1 = guest_texture.Sample(guest_sampler, input.texcoord1 * texture_flags.xy);\n"
              "        float4 t2 = guest_texture.Sample(guest_sampler, input.texcoord2 * texture_flags.xy);\n"
              "        float4 t3 = guest_texture.Sample(guest_sampler, input.texcoord3 * texture_flags.xy);\n"
              "        shaded = 0.5f * (saturate((128.0f / 255.0f) * (t0 + t1))\n"
              "                        + saturate((128.0f / 255.0f) * (t2 + t3)));\n"
              "    } else\n"
            : layout.texcoord_count == 2u
            ? "    if (texture_flags.w > 0.5f) {\n"
              "        float3 rgb = guest_texture.Sample(guest_sampler, input.texcoord1 * texture_flags.xy).rgb;\n"
              "        float alpha = alpha_mask.Sample(mask_sampler, input.texcoord * lighting_flags.zw).a;\n"
              "        shaded = input.color * float4(rgb, alpha);\n"
              "    } else\n" : "",
        /* The fixed-function diffuse default is white. Preserve texture alpha
           before the shared alpha test and SRC_ALPHA blending. */
        has_diffuse ? "*=" : "=");
}

bool drawShaderSource(
    uint32_t fvf,
    const RecompD3dPresenterDrawCommand &pipeline,
    RecompD3dVertexLayout &layout,
    std::string &compiled_source)
{
    if (!recomp_d3d_fvf_layout(fvf, &layout)) {
        return false;
    }

    char source[8192];
    buildDrawShaderSource(layout, source, sizeof source);
    compiled_source = source;
    if (layout.normal_offset != RECOMP_D3D_FVF_ABSENT && !layout.pretransformed &&
        layout.diffuse_offset == RECOMP_D3D_FVF_ABSENT) {
        std::string lighting = "    if (directional_flags.x > 0.5f) {\n"
            "        float3 n = mul(float4(input.normal,0),directional_normals[0]).xyz;\n";
        if (layout.blend_weight_count) {
            lighting += "        if (blend_flags.x > 0.5f) { n=0; float remainder=1;\n";
            for (uint32_t i=0; i<layout.blend_weight_count; ++i) {
                const auto index=std::to_string(i);
                lighting += "n += input.weights["+index+"] * mul(float4(input.normal,0),directional_normals["+index+"]).xyz;\n"
                    "remainder -= input.weights["+index+"];\n";
            }
            lighting += "n += remainder * mul(float4(input.normal,0),directional_normals["+
                std::to_string(layout.blend_weight_count)+"]).xyz; }\n";
        }
        lighting += "        if (directional_flags.y > 0.5f && dot(n,n)>0) n *= rsqrt(dot(n,n));\n"
            "        float3 rgb=directional_base.rgb;\n"
            "        for (uint i=0; i<(uint)directional_flags.z; ++i)\n"
            "            rgb += directional_material.rgb * directional_colors[i].rgb * max(0,dot(n,directional_directions[i].xyz));\n"
            "        output.color.rgb=saturate(rgb);\n"
            "    }\n";
        const auto uv = compiled_source.find("    output.texcoord =");
        if (uv == std::string::npos) return false;
        compiled_source.insert(uv, lighting);
    }
    if (pipeline.program_count) {
        if (fvf != 0x112u) return false;
        std::string body;
        if (!recomp_d3d_vertex_program_source(pipeline.program, pipeline.program_count, body)) return false;
        const auto begin = compiled_source.find("VSOut vs_main(");
        const auto end = compiled_source.find("float4 ps_main(", begin);
        if (begin == std::string::npos || end == std::string::npos) return false;
        compiled_source.replace(begin, end-begin, "VSOut vs_main(VSIn input) { VSOut output;\n" + body + "return output; }\n");
        // Preserve q until the pixel shader so projection follows interpolation.
        const std::string declaration = "struct VSOut {";
        const auto fields = compiled_source.find(declaration);
        if (fields == std::string::npos) return false;
        compiled_source.insert(fields + declaration.size(), "\n    float2 program_q : TEXCOORD5;\n");
        const std::string pixel = "float4 ps_main(VSOut input) : SV_TARGET {";
        const auto sample = compiled_source.find(pixel);
        if (sample == std::string::npos) return false;
        compiled_source.insert(sample + pixel.size(),
            "\n    input.texcoord /= input.program_q.x;\n"
            "    if (reflection_flags.z > 0.5f) input.reflection_coord /= input.program_q.y;\n");

    }
    return true;
}

}
bool recomp_android_draw_hlsl(const RecompD3dPresenterDrawCommand &draw,std::string &source)
{
    RecompD3dVertexLayout layout{};
    return drawShaderSource(draw.fvf,draw,layout,source);
}
