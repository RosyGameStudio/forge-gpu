/*
 * sky.vert.hlsl — Fullscreen quad with inverse VP ray reconstruction
 *
 * Draws a fullscreen quad via SV_VertexID and unpoprojects each vertex
 * position through the inverse view-projection matrix to reconstruct
 * world-space view ray directions.  The fragment shader uses these rays
 * to march through the atmosphere.
 *
 * The key idea: each screen pixel corresponds to a direction in world
 * space.  By placing vertices at the four NDC corners (-1,-1) to (1,1)
 * and multiplying by inv_vp, we get the world-space position on the
 * far plane.  Subtracting the camera position gives the view ray.
 *
 * Six vertices form two triangles covering the screen:
 *   0--1    Triangle 1: 0, 1, 2
 *   | /|    Triangle 2: 2, 1, 3
 *   |/ |    (mapped to vertex IDs 0-5)
 *   2--3
 *
 * Uniform layout (64 bytes):
 *   float4x4 inv_vp   — inverse view-projection matrix
 *
 * SPDX-License-Identifier: Zlib
 */

cbuffer SkyVertUniforms : register(b0, space1)
{
    float4x4 inv_vp;   /* inverse view-projection matrix */
};

struct VSOutput
{
    float4 clip_pos : SV_Position;  /* screen-space quad position      */
    float3 view_ray : TEXCOORD0;    /* world-space ray direction        */
    float2 uv       : TEXCOORD1;    /* UV for post-processing sampling  */
};

VSOutput main(uint vertex_id : SV_VertexID)
{
    VSOutput output;

    /* Generate UV coordinates from vertex ID. */
    float2 uv;
    uv.x = (vertex_id == 1 || vertex_id == 4 || vertex_id == 5) ? 1.0 : 0.0;
    uv.y = (vertex_id == 2 || vertex_id == 3 || vertex_id == 5) ? 1.0 : 0.0;

    /* Flip V so texture row 0 (top) maps to NDC Y = 1 (top). */
    output.uv = float2(uv.x, 1.0 - uv.y);

    /* Map UV [0,1] to NDC [-1,1]. */
    float2 ndc = float2(uv.x * 2.0 - 1.0, uv.y * 2.0 - 1.0);
    output.clip_pos = float4(ndc, 0.0, 1.0);

    /* Unproject NDC position to world space via inverse VP.
     * We use z=1 (far plane) so the resulting position is a point
     * far along the view ray.  The fragment shader subtracts the
     * camera position to get the ray direction. */
    float4 world_pos = mul(inv_vp, float4(ndc.x, ndc.y, 1.0, 1.0));
    output.view_ray = world_pos.xyz / world_pos.w;

    return output;
}
