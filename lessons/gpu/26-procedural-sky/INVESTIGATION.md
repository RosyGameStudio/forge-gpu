# Black Screen Investigation — Lesson 26 LUT Integration

After adding transmittance and multi-scattering LUTs via compute shaders,
the screen is completely black. This document catalogs all identified issues
and the fixes applied.

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

## Verified Correct (NOT the cause)

- **Register space convention:** space2 for fragment textures, space3 for
  fragment uniforms — matches lessons 10, 17, 21
- **Compute space convention:** space0 for sampled textures, space1 for
  RW storage — matches lesson 11
- **Texture usage flags:** `COMPUTE_STORAGE_WRITE | SAMPLER` is correct
- **Separate compute passes:** Required for sync between write and read
- **SkyFragUniforms alignment:** 48 bytes matches HLSL cbuffer
- **LUT UV round-trip:** Bruneton forward mapping is the correct inverse
- **Sub-UV helpers:** Correctly map between parameter and texel-center UV
- **Compiled shader sizes:** All bytecodes are non-empty (3-9 KB)
- **Sampler binding pattern:** Matches lesson 17 (2 fragment samplers)
- **Transmittance LUT:** Contains valid data (visualized successfully)
- **Pipeline and render passes:** Work correctly (diagnostic output visible)
- **Bloom and tone mapping:** Pass through values correctly
