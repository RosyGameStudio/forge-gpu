# Investigation — Lesson 26 LUT Integration

This document catalogs all identified issues and fixes during the LUT
integration for lesson 26. Bugs 1-4 caused the initial black screen.
Bugs 5-7 are rendering correctness issues discovered afterward.

## Root Cause: NaN in multi-scattering LUT at ground level

**Status: FIXED**

**File:** `shaders/multiscatter_lut.comp.hlsl`

When computing the multi-scattering LUT, the altitude was clamped to
`[0, R_ATMO - R_GROUND]`. A clamped altitude of exactly 0 placed
`view_height` at exactly `R_GROUND` (6360 km) — directly on the planet
surface.

From this position, downward-directed rays in the 64-direction sphere
integration had ground intersection distance `t_gnd_near == 0.0`. The
ground check used strict inequality (`> 0.0`), so it failed to clip these
rays:

```hlsl
// WRONG — misses the t_gnd_near == 0.0 boundary case:
if (hits_ground && t_gnd_near > 0.0)
    march_dist = min(march_dist, t_gnd_near - march_start);
```

With the ground check failing, the ray marched through the planet
interior. At negative altitudes, the density functions explode:

```text
altitude = length(pos) - R_GROUND = -500  (inside planet)
exp(-(-500) / 8.0) = exp(62.5) = 1.6e27  (effectively infinity)
```

The scatter integral then computed `infinity * 0 = NaN`:

```hlsl
float3 scatter_integral = (1 - step_ext) / max(extinction, 1e-6);
// scatter_integral = 0 (step_ext = exp(-inf) = 0, so 1 - 0 = 1, / inf = 0)
// BUT: throughput * med.scatter * sun_trans * scatter_integral
//     = 1.0 * inf * sun_trans * 0 = inf * 0 = NaN
```

The NaN propagated into `L_2nd`, then into the final `psi` output. The
bottom rows of the 32x32 multi-scattering LUT (altitude ≈ 0) contained NaN.

In the fragment shader, `sample_multiscatter()` at low altitudes returned
NaN, which contaminated the entire inscatter accumulation, producing a
black screen (NaN renders as black after tone mapping).

**Diagnosis method:** Progressive color-coded diagnostics in the fragment
shader:

1. Direct LUT visualization confirmed pipeline + transmittance LUT work
2. NaN detection (`isnan()`) showed the atmosphere output was NaN
3. Per-component checks identified `sample_multiscatter()` as the NaN source
4. Direct multiscatter LUT visualization showed NaN band at low altitudes

**Fix applied (two changes):**

1. Clamped minimum altitude to 0.01 km (10 m) above ground:

   ```hlsl
   float view_height = R_GROUND + clamp(altitude, 0.01, R_ATMO - R_GROUND);
   ```

2. Changed ground intersection check from strict to inclusive inequality:

   ```hlsl
   if (hits_ground && t_gnd_near >= 0.0)  // was: > 0.0
   ```

## Bug 2: Multi-scattering sphere sampling weight

**Status: FIXED**

**File:** `shaders/multiscatter_lut.comp.hlsl`

The sphere integration weight was wrong. The 4pi solid angle factor should
cancel with the 1/(4pi) isotropic phase, leaving 1/N. But the code applied
only `isotropic_phase * inv_sample_count` = `1/(4pi*N)` instead of `1/N`:

```hlsl
// WRONG — missing 4*pi solid angle factor:
L_2nd += dir_L * isotropic_phase * inv_sample_count;  // = dir_L / (4*pi*N)

// CORRECT — 4*pi cancels with 1/(4*pi), leaving 1/N:
L_2nd += dir_L * inv_sample_count;                     // = dir_L / N
```

**Impact:** Multi-scattering was ~12.57x too dim. Fixed by removing the
`isotropic_phase` variable entirely.

## Bug 3: Dead code in sample_transmittance_lut

**Status: FIXED**

**File:** `shaders/multiscatter_lut.comp.hlsl`

The `sample_transmittance_lut()` function computed `d` using a complex clamp
formula, then immediately overwrote it with a ray-sphere intersection result.
The first computation was dead code. Removed the dead code block.

## Bug 4: Compute uniform buffer dependency (preventive fix)

**Status: FIXED**

The compute shaders originally used a `cbuffer` uniform buffer to receive
LUT dimensions (`lut_width`, `lut_height`). Since the dimensions are
compile-time constants (256x64, 32x32) that never change, the cbuffer was
unnecessary and could introduce timing issues with
`SDL_PushGPUComputeUniformData`. Removed the cbuffer from both compute
shaders and updated pipeline creation to `num_uniform_buffers=0`.

## Bug 5: Missing earth shadow — no Rayleigh sunset colors

**Status: OPEN**

**File:** `shaders/sky.frag.hlsl`, function `atmosphere()`

The atmosphere function does not check whether the sun is occluded by the
planet at each sample point along the view ray. In the Pixel Storm
reference implementation (`ps_sky_lut.cu`, `integrate_scattered_luminance`),
this is done with an explicit ray-sphere test at every march step:

```cuda
// Pixel Storm — earth shadow at each sample point:
f32 t_earth = ray_sphere_intersect_nearest(
    P, sun_dir, earth_o + PS_PLANET_RADIUS_OFFSET * up_vector,
    ap->bottom_radius);
f32 earth_shadow = t_earth >= 0.f ? 0.f : 1.f;

// Smooth transition near the terminator to prevent hard shadow edge:
f32 horizon_fade = __saturatef(sun_zenith_cos_angle * 10.f + 0.5f);
earth_shadow *= horizon_fade;

// Single-scatter is multiplied by earth_shadow;
// multi-scatter is NOT (it's already integrated over the sphere):
float3 S = global_l * (earth_shadow * transmittance_to_sun * phase_scatter
                     + multi_scattered_luminance * medium.scattering);
```

Our code applies `sun_trans` from the transmittance LUT without any
shadow check:

```hlsl
// WRONG — no earth shadow:
float3 S = (scatter_r + scatter_m) * sun_trans * sun_intensity
         + ms * med.scatter * sun_intensity;
```

**Why this matters for sunset colors:**

At sunset (sun at ~4° elevation), sample points along a horizontal view
ray span a range of local sun zenith angles. Points at higher altitudes
or farther from the sun have the sun below their local horizon — the
planet blocks direct sunlight from reaching them.

Without earth shadow, these shadowed points still contribute inscattered
sunlight (using the transmittance LUT value, which is non-zero — see
Bug 6). This adds cold/blue inscattered light from incorrectly-lit
points, diluting the warm Rayleigh scattering colors from the correctly-
lit lower-atmosphere points where sunlight has traveled a long path
and lost its blue component.

With earth shadow, only points where the sun is above the local horizon
contribute single-scatter light. These points receive sunlight that has
traveled through a long atmospheric path (at sunset), preferentially
removing blue wavelengths via Rayleigh scattering. The resulting
`sun_trans` is reddish, producing the warm sunset gradient visible in
the transmittance LUT visualization.

**Required fix (two parts):**

1. Add earth shadow test in the atmosphere loop:

   ```hlsl
   float t_earth_near, t_earth_far;
   bool sun_blocked = ray_sphere_intersect(
       pos, sun_dir, R_GROUND, t_earth_near, t_earth_far);
   float earth_shadow = (sun_blocked && t_earth_near >= 0.0) ? 0.0 : 1.0;

   // Smooth transition near the terminator:
   float horizon_fade = saturate(cos_sun_zenith * 10.0 + 0.5);
   earth_shadow *= horizon_fade;
   ```

2. Apply earth shadow only to the single-scatter term (multi-scatter
   light comes from all directions, already integrated in the LUT):

   ```hlsl
   float3 S = (scatter_r + scatter_m) * sun_trans * earth_shadow * sun_intensity
            + ms * med.scatter * sun_intensity;
   ```

## Bug 6: Transmittance LUT forward mapping clips to ground

**Status: OPEN**

**Files:** `shaders/sky.frag.hlsl` (`transmittance_params_to_uv`),
`shaders/multiscatter_lut.comp.hlsl` (`sample_transmittance_lut`)

The forward Bruneton mapping (params → UV) in both the fragment shader
and the multi-scattering compute shader clips the ray distance `d` to
the ground intersection distance for below-horizon rays:

```hlsl
// Our forward mapping — clips d to ground:
float d = t_far;  // atmosphere distance
if (ray_sphere_intersect(ro, rd, R_GROUND, t_gnd_near, t_gnd_far))
    if (t_gnd_near > 0.0)
        d = t_gnd_near;  // <-- DIFFERENT from compute shader's inverse
```

But the compute shader's inverse mapping (UV → params) uses the full
atmosphere sphere distance, not ground-clipped:

```hlsl
// Compute shader inverse mapping — atmosphere distance (no ground clip):
float d = d_min + uv.x * (d_max - d_min);
cos_zenith = (H*H - rho*rho - d*d) / (2 * view_height * d);
```

The forward and inverse must be exact inverses for the UV round-trip to
work. For above-horizon rays both give the same result (no ground
intersection), so the LUT lookup is correct. For below-horizon rays,
the ground-clipped d is shorter, producing a different UV, which looks
up a DIFFERENT texel than intended.

The Pixel Storm reference uses the atmosphere distance in BOTH mappings
(forward and inverse), making them exact inverses. Below-horizon
sun directions are handled by the explicit earth shadow test (Bug 5),
not by the transmittance LUT itself.

**Required fix:** Remove the ground clipping from the forward mapping
in both `sky.frag.hlsl` and `multiscatter_lut.comp.hlsl`. Match the
Pixel Storm approach: always use the atmosphere sphere distance `d`
for the UV parameterization. Use the quadratic formula directly:

```hlsl
// Correct forward mapping (matches Pixel Storm):
float discriminant = view_height * view_height
    * (cos_zenith * cos_zenith - 1.0) + R_ATMO * R_ATMO;
float d = max(0.0, -view_height * cos_zenith
    + sqrt(max(0.0, discriminant)));
```

With earth shadow (Bug 5 fix) handling below-horizon sun directions,
the below-horizon transmittance values are multiplied by zero anyway,
so the mapping inconsistency becomes harmless. But fixing the mapping
ensures correctness for all cases.

## Bug 7: Bloom persists after the sun sinks below the horizon

**Status: OPEN**

**File:** `shaders/sky.frag.hlsl`, `main.c`

After the sun disc passes below the horizon, bloom glow continues to
appear where the sun used to be. Two contributing factors:

1. **Missing earth shadow (Bug 5):** Without earth shadow, inscattered
   light near the former sun position remains artificially bright,
   exceeding the bloom threshold even after the sun is gone. With earth
   shadow, the inscattered light near the horizon drops to multi-scatter
   levels only, which should fall below the bloom threshold.

2. **Sun disc occlusion check is camera-centric:** The current sun
   occlusion check tests whether a ray from the CAMERA toward the sun
   hits the ground. But the bloom is caused by the bright atmosphere
   near the horizon, not the sun disc itself. This is expected behavior
   once Bug 5 is fixed — the atmosphere near the sun should become
   dimmer as the sun sets, naturally reducing bloom.

**Expected fix:** Fixing Bug 5 (earth shadow) should resolve this
because the bright inscattered light near the sun will correctly
drop to zero as sample points lose direct sunlight.

## Bug 8: Bell curve artifact as sun dips below horizon

**Status: OPEN**

**File:** `shaders/sky.frag.hlsl`

A transient bright band or bell-shaped curve appears in the sky as the
sun transitions from above to below the horizon. This is likely caused
by the interaction of Bugs 5 and 6:

- Without earth shadow, the transition from "sun visible" to "sun in
  Earth's shadow" is driven entirely by the transmittance LUT values,
  which change gradually with cos_sun_zenith. The ground-clipped UV
  mapping (Bug 6) causes an abrupt change in looked-up transmittance
  near cos_sun_zenith = 0, where the mapping transitions between
  ground-clipped and atmosphere-distance modes.

- In Pixel Storm, the `horizon_fade = saturate(cos_sun_zenith * 10 + 0.5)`
  creates a smooth transition over a narrow angular range (~2.9°) around
  the local horizon at each sample point, preventing any sharp boundary.

**Expected fix:** Fixing Bugs 5 and 6 together should eliminate this
artifact. The earth shadow with horizon_fade provides a smooth transition,
and the corrected UV mapping removes the abrupt boundary.

## Verified Correct (NOT the cause)

- **Register space convention:** space2 for fragment textures, space3 for
  fragment uniforms — matches lessons 10, 17, 21
- **Compute space convention:** space0 for sampled textures, space1 for
  RW storage — matches lesson 11
- **Texture usage flags:** `COMPUTE_STORAGE_WRITE | SAMPLER` is correct
- **Separate compute passes:** Required for sync between write and read
- **SkyFragUniforms alignment:** 48 bytes matches HLSL cbuffer
- **Sub-UV helpers:** Correctly map between parameter and texel-center UV
- **Compiled shader sizes:** All bytecodes are non-empty (3-9 KB)
- **Sampler binding pattern:** Matches lesson 17 (2 fragment samplers)
- **Transmittance LUT:** Contains valid data (visualized successfully)
- **Pipeline and render passes:** Work correctly (diagnostic output visible)
- **Bloom and tone mapping:** Pass through values correctly
- **Atmosphere constants:** Match Pixel Storm exactly (Rayleigh, Mie,
  ozone coefficients, scale heights, planet radii)
- **Multi-scattering LUT computation:** Mathematically equivalent to
  Pixel Storm (sphere sampling weights, power series, sub-UV correction)
- **Phase functions:** Rayleigh and Henyey-Greenstein match Pixel Storm
  (Pixel Storm uses a more advanced Draine fog phase for Mie, but the
  standard HG with g=0.8 is correct for teaching purposes)
- **Ozone density profile:** Tent function matches Pixel Storm's
  two-layer piecewise linear formulation

## Reference Implementation Comparison

Key differences between our code and the Pixel Storm reference
(`ps_sky_lut.cu`):

| Feature | Pixel Storm | Lesson 26 | Impact |
|---------|-------------|-----------|--------|
| Earth shadow | `ray_sphere_intersect` + `horizon_fade` | Missing | Bugs 5, 7, 8 |
| Transmittance UV forward | Atmosphere distance (quadratic) | Ground-clipped ray-sphere | Bug 6 |
| Mie phase function | Draine fog phase (4 params) | Henyey-Greenstein (1 param) | Minor visual difference |
| Sun illuminance | Per-channel `float3(10.47, 10.85, 10.91)` | Scalar multiplier `20.0` | Slight color balance |
| Sky-View LUT | 192×108, regenerated each frame | Not implemented (direct ray march) | Performance, not correctness |
| Aerial Perspective LUT | 32×32×32, frustum-aligned volume | Not implemented | No depth fog |
| Sample positioning | `sample_segment_t = 0.3` within step | Midpoint (0.5) within step | Negligible |
| Ground albedo | 0.3 (bounced light at ground) | 0 (no ground bounce) | Slightly dimmer horizon |
