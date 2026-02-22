/*
 * Scene fragment shader — Blinn-Phong with a dim directional fill light.
 *
 * The spotlight (cone falloff, gobo projection, shadow mapping) will be
 * the primary light source once implemented. The directional fill keeps
 * geometry readable in the meantime.
 */

/* Diffuse texture (slot 0). */
Texture2D    diffuse_tex : register(t0, space2);
SamplerState diffuse_smp : register(s0, space2);

cbuffer FragUniforms : register(b0, space3)
{
    float4 base_color;    /* material RGBA              (16 bytes) */
    float3 eye_pos;       /* camera position             (12 bytes) */
    float  has_texture;   /* > 0.5 = sample diffuse_tex   (4 bytes) */
    float  ambient;       /* ambient intensity             (4 bytes) */
    float  fill_intensity; /* directional fill strength    (4 bytes) */
    float  shininess;     /* specular exponent             (4 bytes) */
    float  specular_str;  /* specular strength             (4 bytes) */
    float4 fill_dir;      /* fill light direction (xyz)   (16 bytes) */
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

    /* Dim directional fill — enough to show surface detail */
    float3 L = normalize(-fill_dir.xyz);
    float NdotL = max(dot(N, L), 0.0);
    float3 diffuse = albedo.rgb * NdotL;

    float3 H = normalize(L + V);
    float NdotH = max(dot(N, H), 0.0);
    float3 spec = specular_str * pow(NdotH, shininess);

    total_light += (diffuse + spec) * fill_intensity;

    return float4(total_light, albedo.a);
}
