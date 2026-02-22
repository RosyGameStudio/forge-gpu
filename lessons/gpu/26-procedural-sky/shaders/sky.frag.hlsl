/*
 * sky.frag.hlsl — Physically-based atmospheric scattering (Hillaire)
 *
 * Implements Sébastien Hillaire's single-scattering atmospheric model
 * (EGSR 2020) entirely in the fragment shader.  Each pixel casts a ray
 * from the camera through Earth's atmosphere and accumulates inscattered
 * sunlight via ray marching.
 *
 * Three scattering species:
 *   - Rayleigh: molecular scattering, wavelength-dependent (blue sky)
 *   - Mie: aerosol scattering, forward-peaked (sun halo, hazy horizon)
 *   - Ozone: absorption only, removes orange wavelengths at low sun
 *
 * All atmosphere constants are from Hillaire EGSR 2020 Table 1.
 * Units are in kilometers throughout.
 *
 * Key functions:
 *   1. ray_sphere_intersect — quadratic ray-sphere intersection test
 *   2. rayleigh_phase       — (3/16pi)(1 + cos^2 theta)
 *   3. mie_phase_hg         — Henyey-Greenstein with g=0.8
 *   4. sample_medium        — density and extinction at a given altitude
 *   5. sun_transmittance    — inner march from sample to sun
 *   6. atmosphere           — outer ray march accumulating inscattered light
 *   7. sun_disc             — solar disc with limb darkening
 *
 * Uniform layout (48 bytes):
 *   float3 cam_pos_km       (12 bytes) — camera position in km (planet center)
 *   float  sun_intensity    ( 4 bytes) — sun radiance multiplier
 *   float3 sun_dir          (12 bytes) — normalized direction TO the sun
 *   int    num_steps        ( 4 bytes) — outer ray march step count
 *   float2 resolution       ( 8 bytes) — window size in pixels
 *   int    num_light_steps  ( 4 bytes) — inner (sun) march step count
 *   float  _pad             ( 4 bytes)
 *
 * SPDX-License-Identifier: Zlib
 */

cbuffer SkyFragUniforms : register(b0, space3)
{
    float3 cam_pos_km;       /* camera position in km (planet-centric)       */
    float  sun_intensity;    /* sun radiance multiplier (default 20)         */
    float3 sun_dir;          /* normalized direction toward the sun          */
    int    num_steps;        /* outer ray march steps (default 32)           */
    float2 resolution;       /* window width, height in pixels               */
    int    num_light_steps;  /* inner sun transmittance steps (default 8)    */
    float  _pad;
};

struct PSInput
{
    float4 clip_pos : SV_Position;
    float3 view_ray : TEXCOORD0;
    float2 uv       : TEXCOORD1;
};

/* ════════════════════════════════════════════════════════════════════════════
 * Atmosphere constants — Hillaire EGSR 2020, Table 1
 *
 * All values in km^-1.  The planet is centered at the origin.
 * ════════════════════════════════════════════════════════════════════════════ */

static const float R_GROUND = 6360.0;    /* Earth radius in km             */
static const float R_ATMO   = 6460.0;    /* atmosphere top radius in km    */

/* Rayleigh scattering — molecular scattering by N2 and O2.
 * Wavelength-dependent: short wavelengths (blue) scatter ~16x more than
 * long wavelengths (red), which is why the sky appears blue. */
static const float3 RAYLEIGH_SCATTER = float3(5.802e-3, 13.558e-3, 33.1e-3);
static const float  RAYLEIGH_H       = 8.0;  /* scale height in km */

/* Mie scattering — aerosol particles (dust, water droplets).
 * Nearly wavelength-independent (white/gray).  Strongly forward-peaked
 * due to particle size >> wavelength (Henyey-Greenstein g=0.8). */
static const float MIE_SCATTER = 3.996e-3;
static const float MIE_ABSORB  = 0.444e-3;
static const float MIE_H       = 1.2;  /* scale height in km */
static const float MIE_G       = 0.8;  /* asymmetry parameter */

/* Ozone absorption — concentrated in a layer around 25 km altitude.
 * Absorbs red/green wavelengths, contributing to blue zenith color
 * and the blue-purple tint visible during twilight. */
static const float3 OZONE_ABSORB = float3(0.650e-3, 1.881e-3, 0.085e-3);
static const float  OZONE_CENTER = 25.0;  /* peak altitude in km */
static const float  OZONE_WIDTH  = 15.0;  /* tent half-width in km */

/* Sun angular radius (about 0.53 degrees = 0.00465 radians). */
static const float SUN_ANGULAR_RADIUS = 0.00465;

/* PI constant. */
static const float PI = 3.14159265358979323846;

/* ════════════════════════════════════════════════════════════════════════════
 * Ray-sphere intersection
 *
 * Tests whether a ray (origin ro, direction rd) intersects a sphere of
 * given radius centered at the origin.  Returns the near and far hit
 * distances via t_near and t_far.
 *
 * Uses the standard quadratic formula:
 *   |ro + t*rd|^2 = r^2
 *   t^2(rd.rd) + 2t(ro.rd) + (ro.ro - r^2) = 0
 *
 * Returns false if the ray misses the sphere entirely.
 * ════════════════════════════════════════════════════════════════════════════ */

bool ray_sphere_intersect(float3 ro, float3 rd, float radius,
                          out float t_near, out float t_far)
{
    float a = dot(rd, rd);
    float b = 2.0 * dot(ro, rd);
    float c = dot(ro, ro) - radius * radius;
    float discriminant = b * b - 4.0 * a * c;

    t_near = 0.0;
    t_far  = 0.0;

    if (discriminant < 0.0)
        return false;

    float sqrt_disc = sqrt(discriminant);
    float inv_2a = 1.0 / (2.0 * a);
    t_near = (-b - sqrt_disc) * inv_2a;
    t_far  = (-b + sqrt_disc) * inv_2a;

    return true;
}

/* ════════════════════════════════════════════════════════════════════════════
 * Phase functions
 *
 * Phase functions describe how light is scattered as a function of the
 * angle between the incoming and outgoing directions.
 * ════════════════════════════════════════════════════════════════════════════ */

/* Rayleigh phase function: (3/16pi)(1 + cos^2 theta).
 * Symmetric — scatters equally forward and backward.
 * The cos^2 term means 90-degree scattering is weakest. */
float rayleigh_phase(float cos_theta)
{
    return (3.0 / (16.0 * PI)) * (1.0 + cos_theta * cos_theta);
}

/* Henyey-Greenstein phase function for Mie scattering.
 * g controls the forward peak: g=0 is isotropic, g=0.8 is strongly
 * forward-peaked (light mostly continues in the forward direction).
 * This creates the bright halo around the sun disc. */
float mie_phase_hg(float cos_theta, float g)
{
    float g2 = g * g;
    float denom = 1.0 + g2 - 2.0 * g * cos_theta;
    return (1.0 / (4.0 * PI)) * (1.0 - g2) / (denom * sqrt(denom));
}

/* ════════════════════════════════════════════════════════════════════════════
 * Medium sampling — density and optical properties at a given altitude
 *
 * Each scattering species has a characteristic density profile:
 *   - Rayleigh and Mie: exponential decay with altitude (scale height)
 *   - Ozone: tent function peaking at ~25 km
 *
 * Returns the total scattering and extinction coefficients at position p.
 * ════════════════════════════════════════════════════════════════════════════ */

struct MediumSample
{
    float3 scatter;     /* total scattering coefficient (sigma_s)      */
    float3 extinction;  /* total extinction coefficient (sigma_t)      */
};

MediumSample sample_medium(float3 pos)
{
    MediumSample m;

    /* Altitude above ground in km. */
    float altitude = length(pos) - R_GROUND;

    /* Rayleigh density: exponential falloff with 8 km scale height.
     * At sea level, density = 1.  At 8 km, density = 1/e ≈ 0.37. */
    float rho_rayleigh = exp(-altitude / RAYLEIGH_H);

    /* Mie density: exponential falloff with 1.2 km scale height.
     * Aerosols are concentrated near the surface. */
    float rho_mie = exp(-altitude / MIE_H);

    /* Ozone density: tent function centered at 25 km, width 15 km.
     * This approximation captures the ozone layer's vertical profile. */
    float rho_ozone = max(0.0, 1.0 - abs(altitude - OZONE_CENTER) / OZONE_WIDTH);

    /* Scattering: Rayleigh + Mie (ozone has no scattering). */
    float3 rayleigh_s = RAYLEIGH_SCATTER * rho_rayleigh;
    float  mie_s      = MIE_SCATTER * rho_mie;

    m.scatter = rayleigh_s + float3(mie_s, mie_s, mie_s);

    /* Extinction = scattering + absorption for each species.
     * Rayleigh has zero absorption.  Mie has a small absorption.
     * Ozone is pure absorption (no scattering). */
    float3 rayleigh_t = rayleigh_s;  /* Rayleigh absorption = 0 */
    float  mie_t      = mie_s + MIE_ABSORB * rho_mie;
    float3 ozone_t    = OZONE_ABSORB * rho_ozone;

    m.extinction = rayleigh_t + float3(mie_t, mie_t, mie_t) + ozone_t;

    return m;
}

/* ════════════════════════════════════════════════════════════════════════════
 * Sun transmittance — optical depth from a point to the sun
 *
 * Marches from position pos toward the sun and accumulates extinction.
 * The transmittance is exp(-optical_depth), representing the fraction of
 * sunlight that reaches this point without being scattered or absorbed.
 *
 * This is the "inner loop" of the atmosphere calculation.  Near sunset,
 * light travels a long path through dense atmosphere, causing heavy
 * extinction of blue/green wavelengths — which is why sunsets are orange.
 * ════════════════════════════════════════════════════════════════════════════ */

float3 sun_transmittance(float3 pos, float3 sun_direction)
{
    float t_near, t_far;
    ray_sphere_intersect(pos, sun_direction, R_ATMO, t_near, t_far);

    /* March from pos to atmosphere boundary along sun direction. */
    float step_size = t_far / (float)num_light_steps;
    float3 optical_depth = float3(0.0, 0.0, 0.0);

    for (int i = 0; i < num_light_steps; i++)
    {
        float t = (float(i) + 0.5) * step_size;
        float3 sample_pos = pos + sun_direction * t;

        /* Check if this sample is underground. */
        if (length(sample_pos) < R_GROUND)
            return float3(0.0, 0.0, 0.0);

        MediumSample med = sample_medium(sample_pos);
        optical_depth += med.extinction * step_size;
    }

    return exp(-optical_depth);
}

/* ════════════════════════════════════════════════════════════════════════════
 * Main atmosphere ray march
 *
 * The outer loop: march along the view ray through the atmosphere,
 * accumulating inscattered sunlight at each sample point.
 *
 * At each step:
 *   1. Sample the medium properties (scattering + extinction)
 *   2. Compute transmittance from this point to the sun
 *   3. Compute phase-weighted inscattered radiance
 *   4. Accumulate using Beer-Lambert transmittance
 *
 * The transmittance along the view ray decreases exponentially as we
 * march through denser atmosphere.  This naturally handles:
 *   - Blue sky overhead (short path, little extinction)
 *   - White/orange near horizon (long path, heavy extinction)
 *   - Dark night sky (sun below horizon, no direct illumination)
 * ════════════════════════════════════════════════════════════════════════════ */

float3 atmosphere(float3 ray_origin, float3 ray_dir, out float3 transmittance_out)
{
    /* Intersect with the atmosphere sphere.  If we miss, return black. */
    float t_near, t_far;
    if (!ray_sphere_intersect(ray_origin, ray_dir, R_ATMO, t_near, t_far))
    {
        transmittance_out = float3(1.0, 1.0, 1.0);
        return float3(0.0, 0.0, 0.0);
    }

    /* If the camera is inside the atmosphere, start marching from the
     * camera (t=0), not the entry point. */
    if (t_near < 0.0)
        t_near = 0.0;

    /* Check if the ray hits the ground.  If so, stop the march there. */
    float t_ground_near, t_ground_far;
    if (ray_sphere_intersect(ray_origin, ray_dir, R_GROUND, t_ground_near, t_ground_far))
    {
        if (t_ground_near > 0.0)
            t_far = min(t_far, t_ground_near);
    }

    float step_size = (t_far - t_near) / (float)num_steps;

    /* Cosine of the angle between view ray and sun direction.
     * This is the scattering angle for the phase functions. */
    float cos_theta = dot(ray_dir, sun_dir);

    /* Evaluate phase functions (constant along the ray). */
    float phase_r = rayleigh_phase(cos_theta);
    float phase_m = mie_phase_hg(cos_theta, MIE_G);

    /* Accumulate inscattered radiance and track view transmittance. */
    float3 inscatter = float3(0.0, 0.0, 0.0);
    float3 transmittance = float3(1.0, 1.0, 1.0);

    for (int i = 0; i < num_steps; i++)
    {
        /* Sample at the midpoint of each step for better accuracy. */
        float t = t_near + (float(i) + 0.5) * step_size;
        float3 pos = ray_origin + ray_dir * t;

        /* Sample scattering and extinction at this altitude. */
        MediumSample med = sample_medium(pos);

        /* Transmittance from this sample point to the sun.
         * If the sun is below the horizon from here, this is ~0. */
        float3 sun_trans = sun_transmittance(pos, sun_dir);

        /* Separate Rayleigh and Mie scattering for phase weighting.
         * Each species has its own phase function. */
        float altitude = length(pos) - R_GROUND;
        float rho_r = exp(-altitude / RAYLEIGH_H);
        float rho_m = exp(-altitude / MIE_H);

        float3 scatter_r = RAYLEIGH_SCATTER * rho_r * phase_r;
        float3 scatter_m = float3(MIE_SCATTER, MIE_SCATTER, MIE_SCATTER) * rho_m * phase_m;

        /* In-scattered radiance at this step: sunlight * transmittance_to_sun
         * * scattering * transmittance_along_view * step_size.
         * The sun_intensity multiplier controls overall sky brightness. */
        float3 S = (scatter_r + scatter_m) * sun_trans * sun_intensity;

        /* Beer-Lambert: transmittance decreases exponentially with
         * accumulated extinction (optical depth). */
        float3 step_extinction = exp(-med.extinction * step_size);

        /* Analytical integration of inscattering over the step.
         * Instead of naive: inscatter += S * transmittance * step_size
         * We use: inscatter += S * (1 - exp(-ext*ds)) / ext * transmittance
         * This is more accurate for large step sizes. */
        float3 scatter_integral = (float3(1.0, 1.0, 1.0) - step_extinction)
                                  / max(med.extinction, float3(1e-6, 1e-6, 1e-6));
        inscatter += S * scatter_integral * transmittance;

        /* Update view transmittance for the next step. */
        transmittance *= step_extinction;
    }

    transmittance_out = transmittance;
    return inscatter;
}

/* ════════════════════════════════════════════════════════════════════════════
 * Sun disc with limb darkening
 *
 * Adds a visible sun disc on top of the atmosphere.  The disc uses
 * a smooth edge (smoothstep) and limb darkening — the sun is brighter
 * at the center and dimmer at the edges.  This is caused by viewing
 * through more of the sun's (cooler, dimmer) outer atmosphere at the
 * edges.
 *
 * The empirical limb darkening law: I(r) = 1 - u*(1 - cos(theta))
 * where u ≈ 0.6 for the visible sun and cos(theta) = sqrt(1 - r^2).
 * ════════════════════════════════════════════════════════════════════════════ */

float3 sun_disc(float3 ray_dir, float3 sun_direction, float3 transmittance)
{
    float cos_angle = dot(ray_dir, sun_direction);
    float angle = acos(clamp(cos_angle, -1.0, 1.0));

    /* Smooth edge transition over a small angular range. */
    float edge = smoothstep(SUN_ANGULAR_RADIUS * 1.2, SUN_ANGULAR_RADIUS * 0.9, angle);

    if (edge <= 0.0)
        return float3(0.0, 0.0, 0.0);

    /* Limb darkening: brightness falls off toward the disc edge.
     * r is the fractional radius from disc center (0=center, 1=edge). */
    float r = angle / SUN_ANGULAR_RADIUS;
    float cos_limb = sqrt(max(0.0, 1.0 - r * r));
    float limb_darkening = 1.0 - 0.6 * (1.0 - cos_limb);

    /* The sun's apparent brightness as seen through the atmosphere.
     * We multiply by the view transmittance so the sun dims at sunset. */
    float3 sun_color = float3(1.0, 1.0, 1.0) * sun_intensity * 10.0
                     * limb_darkening * edge * transmittance;

    return sun_color;
}

/* ════════════════════════════════════════════════════════════════════════════
 * Fragment shader entry point
 * ════════════════════════════════════════════════════════════════════════════ */

float4 main(PSInput input) : SV_Target
{
    /* The vertex shader outputs world-space ray directions via the ray
     * matrix (camera basis vectors scaled by FOV/aspect).  Normalize
     * to get the per-pixel view ray direction. */
    float3 ray_dir = normalize(input.view_ray);

    /* March through the atmosphere, accumulating inscattered light. */
    float3 transmittance;
    float3 color = atmosphere(cam_pos_km, ray_dir, transmittance);

    /* Add the sun disc only if the sun is not occluded by the planet.
     * Check if a ray from the camera toward the sun hits the ground
     * sphere — if it does, the planet blocks the sun disc. */
    float t_gnd_near, t_gnd_far;
    bool sun_hits_ground = ray_sphere_intersect(
        cam_pos_km, sun_dir, R_GROUND, t_gnd_near, t_gnd_far);
    if (!sun_hits_ground || t_gnd_near < 0.0)
        color += sun_disc(ray_dir, sun_dir, transmittance);

    return float4(color, 1.0);
}
