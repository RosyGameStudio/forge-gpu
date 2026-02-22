/*
 * Grid fragment shader — procedural anti-aliased grid lines.
 *
 * Uses screen-space derivatives (fwidth) for smooth anti-aliasing and
 * frequency-based fade to prevent moire at low grazing angles.
 *
 * This is the base grid — spotlight and shadow contributions will be
 * added once those systems are implemented.
 */

cbuffer FragUniforms : register(b0, space3)
{
    float4 line_color;
    float4 bg_color;
    float3 eye_pos;
    float  grid_spacing;
    float  line_width;
    float  fade_distance;
    float  _pad0;
    float  _pad1;
};

float4 main(float4 clip_pos : SV_Position, float3 world_pos : TEXCOORD0) : SV_Target
{
    /* Scale world position to grid space */
    float2 grid_uv = world_pos.xz / grid_spacing;

    /* Distance to nearest grid line (0 = on line, 0.5 = between) */
    float2 dist = abs(frac(grid_uv - 0.5) - 0.5);

    /* Screen-space rate of change (pixel footprint in grid space) */
    float2 fw = fwidth(grid_uv);

    /* Anti-aliased line mask */
    float2 aa_line = 1.0 - smoothstep(line_width, line_width + fw, dist);
    float  grid = max(aa_line.x, aa_line.y);

    /* Frequency-based fade: prevent moire when grid cells become sub-pixel */
    float max_fw = max(fw.x, fw.y);
    grid *= 1.0 - smoothstep(0.3, 0.5, max_fw);

    /* Distance fade */
    float cam_dist = length(world_pos - eye_pos);
    float fade = 1.0 - smoothstep(fade_distance * 0.5, fade_distance, cam_dist);
    grid *= fade;

    float3 surface = lerp(bg_color.rgb, line_color.rgb, grid);
    return float4(surface, 1.0);
}
