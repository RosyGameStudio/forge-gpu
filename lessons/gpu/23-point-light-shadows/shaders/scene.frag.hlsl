/*
 * Scene fragment shader — Blinn-Phong with multiple point lights.
 *
 * Each point light contributes diffuse + specular with quadratic attenuation.
 * Shadow sampling will be added later (omnidirectional cube map shadows).
 */

#define MAX_POINT_LIGHTS 4

struct PointLight
{
    float3 position;     /* world-space position   */
    float  intensity;    /* HDR brightness scalar  */
    float3 color;        /* RGB light color        */
    float  _pad;         /* align to 32 bytes      */
};

Texture2D    diffuse_tex : register(t0, space2);
SamplerState diffuse_smp : register(s0, space2);

cbuffer FragUniforms : register(b0, space3)
{
    float4 base_color;                     /* material RGBA            (16) */
    float3 eye_pos;                        /* camera position          (12) */
    float  has_texture;                    /* > 0.5 = textured          (4) */
    float  shininess;                      /* specular exponent         (4) */
    float  ambient;                        /* ambient intensity         (4) */
    float  specular_str;                   /* specular strength         (4) */
    float  _pad;                           /* pad to 16-byte boundary   (4) */
    PointLight lights[MAX_POINT_LIGHTS];   /* point light array        (96) */
};

float4 main(float4 clip_pos : SV_Position,
            float3 world_pos : TEXCOORD0,
            float3 world_nrm : TEXCOORD1,
            float2 uv        : TEXCOORD2) : SV_Target
{
    /* ── Surface color ──────────────────────────────────────────────── */
    float4 albedo = base_color;
    if (has_texture > 0.5)
    {
        albedo *= diffuse_tex.Sample(diffuse_smp, uv);
    }

    /* ── Lighting ───────────────────────────────────────────────────── */
    float3 N = normalize(world_nrm);
    float3 V = normalize(eye_pos - world_pos);

    /* Ambient term */
    float3 total_light = albedo.rgb * ambient;

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
        float3 diffuse = albedo.rgb * NdotL;

        /* Specular (Blinn-Phong) */
        float3 H = normalize(L + V);
        float NdotH = max(dot(N, H), 0.0);
        float3 spec = specular_str * pow(NdotH, shininess);

        total_light += (diffuse + spec) * attenuation * intensity * lights[i].color;
    }

    return float4(total_light, albedo.a);
}
