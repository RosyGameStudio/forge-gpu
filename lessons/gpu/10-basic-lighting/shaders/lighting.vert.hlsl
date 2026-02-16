/*
 * lighting.vert.hlsl — Vertex shader for Blinn-Phong lighting
 *
 * Transforms vertices from object space to clip space AND to world space.
 * The fragment shader needs world-space positions and normals to compute
 * lighting — clip space alone isn't enough because lighting is defined
 * in world coordinates (where the light direction and camera position live).
 *
 * Two matrices:
 *   mvp   — combined Model-View-Projection for clip-space output
 *   model — the model (world) matrix for world-space position and normal
 *
 * Normal transformation:
 *   We use (float3x3)model to transform normals.  This is correct when
 *   the model matrix contains only rotation and uniform scale (no shear
 *   or non-uniform scale).  For non-uniform scale you'd need the
 *   inverse-transpose, but Suzanne has uniform transforms so we keep
 *   it simple.  Normalization happens in the fragment shader after
 *   interpolation (interpolated normals aren't unit length).
 *
 * Vertex attributes (matching ForgeGltfVertex layout):
 *   TEXCOORD0 -> float3 position  (location 0)
 *   TEXCOORD1 -> float3 normal    (location 1)
 *   TEXCOORD2 -> float2 uv        (location 2)
 *
 * Uniform buffer:
 *   register(b0, space1) -> vertex shader uniform slot 0
 *   Contains mvp (64 bytes) + model (64 bytes) = 128 bytes.
 *
 * SPDX-License-Identifier: Zlib
 */

cbuffer VertUniforms : register(b0, space1)
{
    column_major float4x4 mvp;
    column_major float4x4 model;
};

struct VSInput
{
    float3 position : TEXCOORD0;   /* vertex attribute location 0 */
    float3 normal   : TEXCOORD1;   /* vertex attribute location 1 */
    float2 uv       : TEXCOORD2;   /* vertex attribute location 2 */
};

struct VSOutput
{
    float4 clip_pos   : SV_Position; /* clip-space position for rasterizer */
    float2 uv         : TEXCOORD0;   /* texture coordinates               */
    float3 world_norm : TEXCOORD1;   /* world-space normal (not normalized)*/
    float3 world_pos  : TEXCOORD2;   /* world-space position               */
};

VSOutput main(VSInput input)
{
    VSOutput output;

    /* Clip-space position for the rasterizer (same as previous lessons). */
    output.clip_pos = mul(mvp, float4(input.position, 1.0));

    /* World-space position — needed for the view direction (V = eye - P). */
    float4 wp = mul(model, float4(input.position, 1.0));
    output.world_pos = wp.xyz;

    /* World-space normal — transform by the upper-left 3x3 of the model
     * matrix.  NOT normalized here because the rasterizer will interpolate
     * across the triangle, producing non-unit vectors.  The fragment shader
     * normalizes per-pixel. */
    output.world_norm = mul((float3x3)model, input.normal);

    /* Pass UVs through for texture sampling. */
    output.uv = input.uv;

    return output;
}
