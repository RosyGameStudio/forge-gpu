/*
 * Grid fragment shader — procedural anti-aliased grid with distance fade.
 *
 * Uses screen-space derivatives (fwidth) for pixel-perfect anti-aliasing
 * and frequency-based fade to prevent moire at low grazing angles.
 * Includes basic Blinn-Phong shading so the grid reacts to point lights.
 *
 * This version supports multiple point lights — the light array is filled
 * from C code. Unused lights have zero intensity and contribute nothing.
 */

#define MAX_POINT_LIGHTS 4

struct PointLight
{
    float3 position;     /* world-space position   */
    float  intensity;    /* HDR brightness scalar  */
    float3 color;        /* RGB light color        */
    float  _pad;         /* align to 32 bytes      */
};

cbuffer FragUniforms : register(b0, space3)
{
    float4 line_color;              /* grid line color (RGBA, linear)    */
    float4 bg_color;                /* grid background color (RGBA)      */
    float3 eye_pos;                 /* camera position                   */
    float  grid_spacing;            /* world units between lines         */
    float  line_width;              /* line thickness in grid space       */
    float  fade_distance;           /* distance for fade-out              */
    float  ambient;                 /* ambient intensity [0..1]           */
    float  shininess;               /* specular exponent                  */
    float  specular_str;            /* specular intensity [0..1]          */
    float  _pad0;                   /* pad to 16-byte boundary            */
    float  _pad1;
    float  _pad2;
    PointLight lights[MAX_POINT_LIGHTS]; /* point light array             */
};

float4 main(float4 clip_pos : SV_Position, float3 world_pos : TEXCOORD0) : SV_Target
{
    /* ── Procedural grid pattern ────────────────────────────────────── */
    float2 grid_uv = world_pos.xz / grid_spacing;
    float2 dist    = abs(frac(grid_uv - 0.5) - 0.5);
    float2 fw      = fwidth(grid_uv);
    float2 aa_line = 1.0 - smoothstep(line_width, line_width + fw, dist);
    float  grid    = max(aa_line.x, aa_line.y);

    /* Frequency-based fade: prevent moire when grid cells become sub-pixel */
    float max_fw = max(fw.x, fw.y);
    grid *= 1.0 - smoothstep(0.3, 0.5, max_fw);

    /* Distance fade */
    float cam_dist = length(world_pos - eye_pos);
    float fade     = 1.0 - smoothstep(fade_distance * 0.5, fade_distance, cam_dist);
    grid *= fade;

    /* Mix line and background colors */
    float3 surface = lerp(bg_color.rgb, line_color.rgb, grid);

    /* ── Blinn-Phong lighting from point lights ─────────────────────── */
    float3 N = float3(0.0, 1.0, 0.0); /* grid normal is straight up */
    float3 V = normalize(eye_pos - world_pos);

    float3 total_light = surface * ambient;

    for (int i = 0; i < MAX_POINT_LIGHTS; i++)
    {
        float intensity = lights[i].intensity;
        if (intensity <= 0.0) continue;

        float3 L = lights[i].position - world_pos;
        float  d = length(L);
        L /= d; /* normalize */

        /* Quadratic attenuation */
        float attenuation = 1.0 / (1.0 + 0.35 * d + 0.44 * d * d);

        /* Diffuse */
        float NdotL = max(dot(N, L), 0.0);
        float3 diffuse = surface * NdotL;

        /* Specular (Blinn-Phong) */
        float3 H = normalize(L + V);
        float NdotH = max(dot(N, H), 0.0);
        float3 spec = specular_str * pow(NdotH, shininess);

        total_light += (diffuse + spec) * attenuation * intensity * lights[i].color;
    }

    return float4(total_light, 1.0);
}
