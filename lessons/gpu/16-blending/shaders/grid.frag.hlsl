/*
 * grid.frag.hlsl — Fragment shader for the procedural anti-aliased grid
 *
 * Generates a grid pattern using screen-space derivatives (fwidth)
 * for anti-aliased lines, with distance-based fade to prevent Moire
 * patterns at the horizon.  Same technique as Lesson 12.
 *
 * Fragment uniform:
 *   register(b0, space3) -> grid appearance parameters (48 bytes)
 *
 * SPDX-License-Identifier: Zlib
 */

cbuffer GridFragUniforms : register(b0, space3)
{
    float4 line_color;     /* grid line color (linear space)            */
    float4 bg_color;       /* background color between lines            */
    float  grid_spacing;   /* world-space distance between grid lines   */
    float  line_width;     /* grid line thickness in world units        */
    float  fade_distance;  /* distance at which grid fades out          */
    float  _pad;
};

struct PSInput
{
    float4 clip_pos  : SV_Position;
    float3 world_pos : TEXCOORD0;
};

float4 main(PSInput input) : SV_Target
{
    /* Compute grid coordinates from world XZ position */
    float2 coord = input.world_pos.xz / grid_spacing;

    /* Distance to the nearest grid line in each axis.
     * fwidth() gives the screen-space rate of change — dividing by it
     * converts the world-space distance to pixel-space, which is what
     * we need for anti-aliased step functions. */
    float2 grid_dist = abs(frac(coord - 0.5) - 0.5) / fwidth(coord);
    float nearest = min(grid_dist.x, grid_dist.y);

    /* Convert distance-to-line into an alpha: 0 = on line, 1 = off line.
     * Clamping to [0,1] gives us a smooth anti-aliased transition. */
    float alpha = 1.0 - saturate(nearest);

    /* Fade grid lines with distance to prevent aliasing at the horizon */
    float dist = length(input.world_pos.xz);
    float fade = 1.0 - saturate(dist / fade_distance);
    alpha *= fade;

    return lerp(bg_color, line_color, alpha);
}
