---
name: procedural-sky
description: Add a physically-based procedural sky with atmospheric scattering (Rayleigh, Mie, ozone) to an SDL GPU project using per-pixel ray marching
user_invokable: true
---

# Procedural Sky (Hillaire)

Add a physically-based atmospheric sky to an SDL GPU project. Implements
Sébastien Hillaire's single-scattering model (EGSR 2020) with per-pixel
ray marching through Earth's atmosphere, producing correct sky colors at
any time of day.

## When to use

- Outdoor scenes that need a dynamic sky background
- Time-of-day systems (sunrise, noon, sunset, twilight, night)
- Replacing static skybox textures with physically correct skies
- Scenes where the sun position changes dynamically
- Any project needing atmospheric scattering (aerial perspective)

## Architecture

### Render pipeline

4 pipelines, 4 pass types per frame:

1. **Sky pass** — Fullscreen quad, atmosphere ray march → HDR target
2. **Bloom downsample** (5×) — 13-tap Jimenez with Karis → mip chain
3. **Bloom upsample** (4×) — 9-tap tent, additive blend → mip 0
4. **Tonemap pass** — HDR + bloom → swapchain (ACES filmic)

### Key shader: sky.frag.hlsl

All atmosphere constants are `static const` in the shader. Uniforms
are camera position, sun direction, and march step counts.

Core functions (implement in this order):

```hlsl
/* 1. Ray-sphere test — find atmosphere entry/exit */
bool ray_sphere_intersect(float3 ro, float3 rd, float radius,
                          out float t_near, out float t_far);

/* 2. Phase functions — angular scattering distribution */
float rayleigh_phase(float cos_theta);      /* (3/16pi)(1+cos^2 theta) */
float mie_phase_hg(float cos_theta, float g); /* Henyey-Greenstein g=0.8 */

/* 3. Medium sampling — density at altitude */
MediumSample sample_medium(float3 pos);     /* returns scatter + extinction */

/* 4. Sun transmittance — inner march from sample to sun */
float3 sun_transmittance(float3 pos, float3 sun_dir);

/* 5. Main march — outer loop accumulating inscattered light */
float3 atmosphere(float3 ray_origin, float3 ray_dir,
                  out float3 transmittance);

/* 6. Sun disc — rendered on top with limb darkening */
float3 sun_disc(float3 ray_dir, float3 sun_dir, float3 transmittance);
```

### Atmosphere constants (Hillaire EGSR 2020, Table 1)

```hlsl
static const float R_GROUND = 6360.0;  /* Earth radius (km) */
static const float R_ATMO   = 6460.0;  /* atmosphere top (km) */

/* Rayleigh — molecular scattering, wavelength-dependent */
static const float3 RAYLEIGH_SCATTER = float3(5.802e-3, 13.558e-3, 33.1e-3);
static const float  RAYLEIGH_H = 8.0;  /* scale height (km) */

/* Mie — aerosol scattering, nearly white */
static const float MIE_SCATTER = 3.996e-3;
static const float MIE_ABSORB  = 0.444e-3;
static const float MIE_H       = 1.2;
static const float MIE_G       = 0.8;  /* forward-peak asymmetry */

/* Ozone — absorption only (no scattering) */
static const float3 OZONE_ABSORB = float3(0.650e-3, 1.881e-3, 0.085e-3);
static const float  OZONE_CENTER = 25.0;  /* peak altitude (km) */
static const float  OZONE_WIDTH  = 15.0;  /* tent half-width (km) */
```

### Vertex shader: ray matrix reconstruction

```hlsl
cbuffer SkyVertUniforms : register(b0, space1) {
    float4x4 ray_matrix; /* maps NDC to world-space directions */
};

/* Map NDC to world-space ray direction via the ray matrix */
float4 world_pos = mul(ray_matrix, float4(ndc.x, ndc.y, 1.0, 1.0));
output.view_ray = world_pos.xyz / world_pos.w;
```

### C-side uniform structures

```c
/* Sky vertex: ray matrix mapping NDC to world-space directions (64 bytes).
 * Built from camera basis vectors scaled by FOV/aspect — avoids precision
 * loss from inverse VP at planet-centric coordinates (~6360 km). */
typedef struct SkyVertUniforms {
    mat4 ray_matrix;
} SkyVertUniforms;

/* Sky fragment: camera + sun + march params (48 bytes) */
typedef struct SkyFragUniforms {
    float cam_pos_km[3];
    float sun_intensity;     /* default: 20.0 */
    float sun_dir[3];
    int   num_steps;         /* default: 32 */
    float resolution[2];
    int   num_light_steps;   /* default: 8 */
    float _pad;
} SkyFragUniforms;
```

### Sun direction from angles

```c
/* elevation: radians above horizon (0=horizon, pi/2=zenith)
 * azimuth: radians from east (increases CCW viewed from above) */
static vec3 sun_direction_from_angles(float elevation, float azimuth) {
    float cos_el = SDL_cosf(elevation);
    vec3 dir;
    dir.x = cos_el * SDL_cosf(azimuth);
    dir.y = SDL_sinf(elevation);
    dir.z = cos_el * SDL_sinf(azimuth);
    return dir;
}
```

### Camera setup

Planet-centric coordinates in km. Camera starts at surface:

```c
#define CAM_START_Y 6360.001f  /* 1 meter above sea level */
#define CAM_SPEED   0.2f       /* km/s base speed */
#define NEAR_PLANE  0.0001f    /* 0.1 meters in km */
#define FAR_PLANE   1000.0f    /* 1000 km */
```

## Key API calls

| Function | Purpose |
|----------|---------|
| `SDL_CreateGPUTexture` | HDR render target (R16G16B16A16_FLOAT) |
| `SDL_CreateGPUSampler` | Linear/clamp samplers for HDR and bloom |
| `SDL_CreateGPUGraphicsPipeline` | 4 pipelines (sky, downsample, upsample, tonemap) |
| `SDL_PushGPUVertexUniformData` | Push ray matrix to sky vertex shader |
| `SDL_PushGPUFragmentUniformData` | Push sky params, bloom params, tonemap params |
| `SDL_BindGPUFragmentSamplers` | Bind source textures for bloom and tonemap |
| `SDL_DrawGPUPrimitives` | Fullscreen quad (6 verts, no vertex buffer) |
| `quat_right`, `quat_up`, `quat_forward` | Camera basis vectors for ray matrix |
| `quat_from_euler` | Camera orientation from yaw/pitch |

## Bloom pipeline (reused from Lesson 22)

The sky needs HDR + bloom because the sun disc emits values far
exceeding 1.0. See the [bloom skill](../bloom/SKILL.md) for full
bloom downsample/upsample implementation details.

Key adaptation for sky: the bloom threshold catches the bright sun
disc and creates a natural glow halo.

## Common mistakes

1. **Forgetting to normalize the view ray** — The fragment shader must
   `normalize(input.view_ray - cam_pos_km)`. Without normalization, the
   ray march step sizes are wrong and the sky appears stretched.

2. **Wrong coordinate system** — All constants are in km. If you place the
   camera at `(0, 0, 0)` instead of `(0, 6360.001, 0)`, it's inside the
   planet. The camera Y must be ≥ R_GROUND.

3. **Missing HDR render target** — Rendering the sky directly to the LDR
   swapchain clips all values above 1.0, losing the sun disc and any
   bloom. Always render to R16G16B16A16_FLOAT first.

4. **Bloom upsample without additive blend** — The upsample pipeline must
   use ONE+ONE additive blending with LOADOP_LOAD. Without this, each
   upsample pass overwrites the downsample data instead of accumulating.

5. **Sun disc too bright without bloom** — The sun outputs
   `sun_intensity * 10.0`, which is 200× the sky brightness. Without
   bloom and tone mapping, this appears as a hard white circle with no
   soft glow.

6. **Ray matrix not updated on resize** — If the window resizes, the
   aspect ratio changes. The ray matrix must be recomputed every frame
   from the current window dimensions.

## Reference implementation

- [Lesson 26 main.c](../../../lessons/gpu/26-procedural-sky/main.c)
- [sky.frag.hlsl](../../../lessons/gpu/26-procedural-sky/shaders/sky.frag.hlsl)
- [Lesson 22 — Bloom](../../../lessons/gpu/22-bloom/) — HDR + bloom pipeline
- Hillaire, S. (2020). *A Scalable and Production Ready Sky and
  Atmosphere Rendering Technique.* EGSR 2020.
