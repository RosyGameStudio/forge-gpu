/*
 * tonemap.frag.hlsl — Tone mapping with bloom compositing
 *
 * Samples both the HDR render target and the bloom result, combines
 * them, and tone maps the result to LDR.
 *
 * Bloom is added BEFORE tone mapping so that bloom values participate
 * in the same HDR-to-LDR compression — highlights roll off naturally.
 *
 * Operators:
 *   0 — No tone mapping (clamp to [0,1])
 *   1 — Reinhard: x / (x + 1)
 *   2 — ACES filmic (Narkowicz approximation)
 *
 * Gamma: outputs LINEAR values. The sRGB swapchain applies gamma.
 *
 * SPDX-License-Identifier: Zlib
 */

Texture2D    hdr_texture   : register(t0, space2);
SamplerState hdr_smp       : register(s0, space2);

Texture2D    bloom_texture : register(t1, space2);
SamplerState bloom_smp     : register(s1, space2);

cbuffer TonemapUniforms : register(b0, space3)
{
    float  exposure;         /* scene exposure multiplier */
    uint   tonemap_mode;     /* 0=clamp, 1=Reinhard, 2=ACES */
    float  bloom_intensity;  /* bloom contribution strength */
    float  _pad;
};

struct PSInput
{
    float4 clip_pos : SV_Position;
    float2 uv       : TEXCOORD0;
};

float3 tonemap_reinhard(float3 hdr)
{
    return hdr / (hdr + 1.0);
}

float3 tonemap_aces(float3 hdr)
{
    float a = 2.51;
    float b = 0.03;
    float c = 2.43;
    float d = 0.59;
    float e = 0.14;
    return saturate((hdr * (a * hdr + b)) / (hdr * (c * hdr + d) + e));
}

float4 main(PSInput input) : SV_Target
{
    float3 hdr = hdr_texture.Sample(hdr_smp, input.uv).rgb;

    /* Add bloom before tone mapping */
    float3 bloom = bloom_texture.Sample(bloom_smp, input.uv).rgb;
    hdr += bloom * bloom_intensity;

    /* Apply exposure */
    hdr *= exposure;

    /* Tone map */
    float3 ldr;
    if (tonemap_mode == 1)
    {
        ldr = tonemap_reinhard(hdr);
    }
    else if (tonemap_mode == 2)
    {
        ldr = tonemap_aces(hdr);
    }
    else
    {
        ldr = saturate(hdr);
    }

    return float4(ldr, 1.0);
}
