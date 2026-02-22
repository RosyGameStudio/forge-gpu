# Lesson 26 — Procedural Sky (Hillaire)

A physically-based atmospheric sky rendered entirely in the fragment shader
using per-pixel ray marching through Earth's atmosphere, with Rayleigh,
Mie, and ozone scattering.

## What you'll learn

- Per-pixel ray marching through a planetary atmosphere
- Rayleigh scattering (wavelength-dependent — why the sky is blue)
- Mie scattering (forward-peaked — sun halo and hazy horizons)
- Ozone absorption (blue-purple tint during twilight)
- The Beer-Lambert law for light extinction along a path
- Phase functions (Rayleigh symmetric, Henyey-Greenstein forward)
- Ray-sphere intersection for atmosphere entry/exit
- Inverse view-projection for world-space ray reconstruction
- HDR rendering with Jimenez dual-filter bloom
- ACES filmic tone mapping with exposure control
- Sun disc rendering with limb darkening
- Quaternion fly camera in planet-centric (km) coordinates

## Result

<!-- ![Lesson 26 screenshot](assets/screenshot.png) -->

A real-time procedural sky with physically correct colors at any time of
day. The sun creates natural bloom through HDR values exceeding 1.0.
Arrow keys change the sun angle — watch the sky transition from deep blue
noon to orange sunset to dark twilight. WASD + mouse lets you fly through
the atmosphere in kilometers.

## Controls

| Key | Action |
|-----|--------|
| WASD / Space / C | Fly camera (Space = up, C = down) |
| LShift | 10× speed boost |
| Mouse | Look around |
| Left/Right arrows | Sun azimuth |
| Up/Down arrows | Sun elevation |
| T | Toggle auto sun rotation |
| 1/2/3 | Tonemap: Clamp / Reinhard / ACES |
| =/+ | Increase exposure |
| - | Decrease exposure |
| B | Toggle bloom |
| Escape | Release mouse / quit |

## Key concepts

### Why per-pixel ray marching?

Pre-baked skybox textures are static — they capture one moment in time
and can't change with the sun angle. Lookup tables (LUTs) pre-compute
atmosphere colors into textures, which is fast but hides the physics.

This lesson computes the sky per pixel, every frame. A ray from the
camera through each pixel marches through Earth's atmosphere, accumulating
scattered sunlight at each step. The result is a physically correct sky
that responds naturally to any sun angle, camera altitude, or atmosphere
configuration — all from a single fragment shader.

The approach follows Sébastien Hillaire's single-scattering model
(EGSR 2020), which achieves real-time performance with ~32 outer steps
and ~8 inner steps per pixel.

### Atmosphere structure

![Atmosphere layers](assets/atmosphere_layers.png)

Earth's atmosphere extends from the ground (radius 6360 km) to
approximately 100 km altitude (radius 6460 km). Three species contribute
to light scattering and absorption:

| Species | Scattering σ\_s (km⁻¹) | Absorption σ\_a (km⁻¹) | Scale height |
|---------|------------------------|------------------------|--------------|
| Rayleigh | (5.802, 13.558, 33.1)×10⁻³ | 0 | 8 km |
| Mie | 3.996×10⁻³ | 0.444×10⁻³ | 1.2 km |
| Ozone | 0 | (0.650, 1.881, 0.085)×10⁻³ | Tent @ 25 km |

These constants come from Hillaire's EGSR 2020 paper (Table 1) and are
encoded as `static const` values in the shader.

### Density profiles

![Density profiles](assets/density_profiles.png)

Each species has a characteristic density profile that determines how
concentrated it is at different altitudes:

- **Rayleigh** — Exponential decay with 8 km scale height. At sea level
  the density is 1.0; at 8 km it drops to 1/e ≈ 0.37. This models
  molecular scattering by nitrogen and oxygen.

- **Mie** — Exponential decay with 1.2 km scale height. Aerosol particles
  (dust, water droplets) are concentrated near the surface, falling off
  rapidly with altitude.

- **Ozone** — A tent function centered at 25 km with 15 km half-width.
  This approximates the ozone layer, which absorbs red/green wavelengths
  and contributes to the blue zenith and purple twilight colors.

```hlsl
float rho_rayleigh = exp(-altitude / 8.0);  /* exponential decay */
float rho_mie      = exp(-altitude / 1.2);  /* concentrated near surface */
float rho_ozone    = max(0, 1 - abs(altitude - 25.0) / 15.0);  /* tent */
```

### Ray-sphere intersection

![Ray-sphere intersection](assets/ray_sphere_intersection.png)

Every pixel starts by casting a ray from the camera and finding where it
enters and exits the atmosphere sphere. This is a standard quadratic
ray-sphere test:

Given a ray with origin **o** and direction **d**, and a sphere of
radius r centered at the origin:

```text
|o + t·d|² = r²
t²(d·d) + 2t(o·d) + (o·o - r²) = 0
```

The discriminant tells us whether the ray hits the sphere, and the two
roots give us `t_near` (entry) and `t_far` (exit). If the camera is
inside the atmosphere, we start marching from t=0 instead of the entry
point. If the ray hits the ground sphere, we stop the march there.

### Scattering geometry

![Scattering geometry](assets/scattering_geometry.png)

At each sample point along the view ray, sunlight arrives from the sun
direction and scatters toward the camera. The **scattering angle θ** is
the angle between the sun direction and the view ray direction:

```hlsl
float cos_theta = dot(ray_dir, sun_dir);
```

This angle determines how much light is redirected toward the viewer,
as described by the phase functions.

### Phase functions

![Phase functions](assets/phase_functions.png)

Phase functions describe the angular distribution of scattered light.
Two species, two very different behaviors:

**Rayleigh phase function** — symmetric scattering by molecules:

```hlsl
float rayleigh_phase(float cos_theta) {
    return (3.0 / (16.0 * PI)) * (1.0 + cos_theta * cos_theta);
}
```

The cos²θ term means forward and backward scattering are equally strong,
while 90° scattering is weakest. This is why the blue sky is relatively
uniform.

**Henyey-Greenstein phase function** — forward-peaked scattering by
aerosols:

```hlsl
float mie_phase_hg(float cos_theta, float g) {
    float g2 = g * g;
    float denom = 1.0 + g2 - 2.0 * g * cos_theta;
    return (1.0 / (4.0 * PI)) * (1.0 - g2) / (denom * sqrt(denom));
}
```

With g=0.8, light is strongly peaked in the forward direction. This
creates the bright halo (aureole) around the sun and the white glare
near the horizon.

### The ray march

![Ray march diagram](assets/ray_march_diagram.png)

The outer loop marches along the view ray from the camera to the
atmosphere boundary, sampling the medium at regular intervals:

```hlsl
for (int i = 0; i < num_steps; i++) {
    float t = t_near + (float(i) + 0.5) * step_size;
    float3 pos = ray_origin + ray_dir * t;

    MediumSample med = sample_medium(pos);
    float3 sun_trans = sun_transmittance(pos, sun_dir);

    /* Phase-weighted inscattered radiance */
    float3 S = (scatter_r + scatter_m) * sun_trans * sun_intensity;

    /* Analytical integration for better accuracy */
    float3 step_ext = exp(-med.extinction * step_size);
    float3 integral = (1.0 - step_ext) / max(med.extinction, 1e-6);
    inscatter += S * integral * transmittance;

    transmittance *= step_ext;
}
```

At each step:

1. **Sample the medium** — get scattering and extinction coefficients
   from the density profiles
2. **Compute sun transmittance** — inner march from this point toward
   the sun to find how much sunlight reaches here
3. **Accumulate inscattered light** — multiply by phase functions and
   the current view transmittance
4. **Update transmittance** — Beer-Lambert law reduces it exponentially

The analytical integration `(1 - exp(-ext*ds)) / ext` is more accurate
than the naive `ext * ds` approximation, especially for large step sizes.

### Sun transmittance and sunset colors

![Sun transmittance](assets/sun_transmittance.png)

The inner march determines how much sunlight reaches each sample point.
At noon, sunlight travels a short path straight down through the
atmosphere — little extinction occurs, and the sky is bright blue
(Rayleigh scattering favors short wavelengths).

At sunset, sunlight must travel a long path through dense low-altitude
atmosphere. The path length is much greater, causing heavy extinction.
Blue and green wavelengths are scattered away first (they have the
highest Rayleigh coefficients), leaving predominantly red and orange
light — this is why sunsets are orange.

### Sun disc with limb darkening

![Sun limb darkening](assets/sun_limb_darkening.png)

The sun disc is rendered on top of the atmosphere, using a smooth-edged
circle at the sun's angular radius (0.53°). **Limb darkening** makes the
disc brighter at the center and dimmer at the edges:

```hlsl
float cos_limb = sqrt(max(0, 1 - r * r));
float limb_darkening = 1 - 0.6 * (1 - cos_limb);
```

This is a physical effect — at the sun's edge, we view through more of
the sun's cooler outer layers, which emit less light. The coefficient
u=0.6 is an empirical value for the visible spectrum.

The sun's color is modulated by the view transmittance, so it naturally
reddens at sunset — the same extinction that colors the sky also colors
the sun.

### Time of day

![Time of day colors](assets/time_of_day_colors.png)

The sun elevation angle controls the entire mood of the sky:

- **Night** (sun below -10°) — no direct illumination, dark sky
- **Twilight** (-10° to 0°) — ozone absorption creates purple tones
- **Sunrise/sunset** (0° to 5°) — long atmospheric paths produce orange
- **Daytime** (5° to 90°) — short paths, Rayleigh dominates, blue sky

All of these transitions happen naturally from the physics — no artistic
color grading needed.

### Render pipeline

![Render pipeline](assets/render_pipeline.png)

The rendering follows a multi-pass HDR pipeline:

1. **Sky pass** — Ray march the atmosphere into an R16G16B16A16_FLOAT
   HDR render target. The sun disc can produce values far exceeding 1.0.

2. **Bloom downsample** (5 passes) — Jimenez 13-tap filter progressively
   halves the resolution. The first pass applies a brightness threshold
   and Karis averaging to suppress firefly artifacts from the bright sun.

3. **Bloom upsample** (4 passes) — 9-tap tent filter with additive
   blending accumulates the glow back up the mip chain.

4. **Tone map** — Combines the HDR sky with bloom, applies exposure
   control, and maps to displayable range with ACES filmic tone mapping.

This is the same bloom pipeline from
[Lesson 22](../22-bloom/README.md), adapted for the procedural sky.

### Inverse view-projection ray reconstruction

The vertex shader places a fullscreen quad and uses the inverse
view-projection matrix to unproject each vertex to world space:

```hlsl
float4 world_pos = mul(inv_vp, float4(ndc.x, ndc.y, 1.0, 1.0));
output.view_ray = world_pos.xyz / world_pos.w;
```

The fragment shader then computes the view ray direction:

```hlsl
float3 ray_dir = normalize(input.view_ray - cam_pos_km);
```

This avoids passing separate camera matrices to the fragment shader —
the hardware interpolation of `view_ray` across the quad gives us
correct per-pixel ray directions.

### Planet-centric coordinates

The camera works in kilometers with the planet center at the origin.
The initial position `(0, 6360.001, 0)` places the camera 1 meter
above sea level at the equator. Movement speed is 0.2 km/s (200 m/s)
with a 10× boost when holding Shift.

This coordinate system matches the atmosphere constants directly — the
ground sphere radius is 6360 km and the atmosphere top is 6460 km. No
unit conversion needed between the C code and the shader.

## Code structure

```text
26-procedural-sky/
├── main.c                        Application with 4 pipelines
├── CMakeLists.txt                Build configuration
├── README.md                     This file
└── shaders/
    ├── sky.vert.hlsl             Fullscreen quad + inv_vp ray reconstruction
    ├── sky.frag.hlsl             Atmospheric scattering ray march
    ├── fullscreen.vert.hlsl      Standard SV_VertexID quad (bloom/tonemap)
    ├── bloom_downsample.frag.hlsl  13-tap Jimenez with Karis averaging
    ├── bloom_upsample.frag.hlsl    9-tap tent filter
    └── tonemap.frag.hlsl          ACES + bloom compositing
```

**4 pipelines:** sky, downsample, upsample, tonemap

**Per-frame render sequence:**

1. Sky pass → HDR target (LOADOP_DONT_CARE, no depth)
2. 5× bloom downsample (HDR → bloom_mips[0..4])
3. 4× bloom upsample (bloom_mips[4] → bloom_mips[0], additive ONE+ONE)
4. Tonemap pass (HDR + bloom_mips[0] → swapchain)

## Build and run

```bash
cmake --build build --config Debug --target 26-procedural-sky
```

## AI skill

The [procedural-sky skill](../../../.claude/skills/procedural-sky/SKILL.md)
teaches Claude how to add atmospheric scattering to any SDL GPU project.
Invoke it with `/procedural-sky` in Claude Code.

## What's next

This lesson computes single scattering — each light ray scatters at most
once before reaching the camera. Real atmospheres involve multiple
scattering (light bouncing between molecules many times), which fills in
shadows and brightens the horizon. Exercise 3 introduces a simple
approximation; for production quality, explore Hillaire's multi-scattering
LUT approach.

Combining this sky with 3D geometry requires **aerial perspective** — applying
the atmosphere integral between the camera and each surface point. This is
explored in Exercise 5 and is how production engines integrate atmospheric
scattering with scene rendering.

## Exercises

1. **Increase march steps** — Change `NUM_VIEW_STEPS` from 32 to 64.
   Compare the visual quality, especially near the horizon at sunset.
   What is the performance cost? At what step count does the sky look
   "good enough"?

2. **Add ground color** — When the view ray hits the ground sphere,
   return a ground albedo (e.g., brown earth) lit by the sun
   transmittance at the hit point instead of black. How does the
   ground color change at sunset?

3. **Multiple scattering approximation** — The current model only
   computes single scattering (light scattered once toward the camera).
   Add a simple multi-scattering approximation: after computing single
   scatter, add a fraction (e.g., 0.3×) of the inscattered light with
   an isotropic phase function. This brightens shadowed areas and
   reduces the dark band near the horizon at sunset.

4. **Atmosphere LUT** — Pre-compute the transmittance function into a
   2D lookup table (altitude × sun zenith angle) in a compute shader.
   Sample this LUT instead of doing the inner march per pixel. Compare
   performance and quality. This is the approach used in production
   engines.

5. **Aerial perspective** — Add 3D geometry (a terrain mesh) and apply
   the atmosphere between the camera and each surface point. Objects
   far away should fade into the sky color. This requires evaluating
   the atmosphere integral from camera to the object distance, not to
   the atmosphere boundary.

## References

- Hillaire, S. (2020). *A Scalable and Production Ready Sky and
  Atmosphere Rendering Technique.* EGSR 2020.
- Jimenez, J. (2014). *Next Generation Post Processing in Call of Duty:
  Advanced Warfare.* SIGGRAPH 2014.
- Narkowicz, K. (2015). *ACES Filmic Tone Mapping Curve.* Blog post.
- [Lesson 22 — Bloom](../22-bloom/README.md) — Jimenez bloom pipeline
- [Lesson 21 — HDR Tone Mapping](../21-hdr-tone-mapping/README.md) —
  HDR rendering and tone mapping fundamentals
- [Math Lesson 06 — Projections](../../math/06-projections/README.md) —
  Perspective matrix and inverse projection
