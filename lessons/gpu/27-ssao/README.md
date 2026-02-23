# Lesson 27 — SSAO (Screen-Space Ambient Occlusion)

Screen-space ambient occlusion estimates how much ambient light reaches each
pixel by sampling the depth buffer in a hemisphere around the surface normal.
The result darkens crevices, corners, and contact areas where light is naturally
blocked by nearby geometry — adding depth and grounding to a scene without
expensive global illumination.

![SSAO](assets/screenshot.png)

## What you will learn

- How SSAO works: hemisphere kernel sampling in view space
- Depth buffer reconstruction via inverse projection
- Building a TBN matrix from a random noise vector (Gram-Schmidt)
- Multiple render targets (MRT) for the G-buffer (color + view normals)
- Box blur to smooth the noise tile pattern
- Compositing the AO factor with the lit scene
- Interleaved Gradient Noise (IGN) for dithering and jitter

## Prerequisites

- [Lesson 06 — Depth & 3D](../06-depth-and-3d/) (depth buffers)
- [Lesson 10 — Basic Lighting](../10-basic-lighting/) (Blinn-Phong)
- [Lesson 15 — Shadow Maps](../15-cascaded-shadow-maps/) (depth-only passes)
- [Lesson 21 — HDR & Tone Mapping](../21-hdr-tone-mapping/) (render-to-texture)
- [Lesson 25 — Shader Noise](../25-shader-noise/) (IGN dithering)
- [Math Lesson 05 — Matrices](../../math/05-matrices/) (inverse matrices)

## Architecture — 5 render passes per frame

```text
1. Shadow pass      → shadow_depth (D32_FLOAT, 2048x2048)
2. Geometry pass    → scene_color (R8G8B8A8_UNORM) + view_normals (R16G16B16A16_FLOAT) + scene_depth (D32_FLOAT)
3. SSAO pass        → ssao_raw (R8_UNORM, fullscreen quad)
4. Blur pass        → ssao_blurred (R8_UNORM, fullscreen quad, 4x4 box blur)
5. Composite pass   → swapchain (fullscreen quad, combines color + AO)
```

The geometry pass uses **multiple render targets** (MRT) — a single draw call
writes to two color attachments simultaneously. The fragment shader outputs both
the lit scene color and the view-space normal, which the SSAO pass reads as
input textures.

## The SSAO algorithm

### Step 1: Generate the sampling kernel (CPU, once at init)

We generate 64 random sample vectors inside a unit hemisphere oriented along
+Z (tangent space). Each sample is scaled with a quadratic falloff that
concentrates more samples near the surface, where close-range occlusion detail
matters most:

```c
float t = (float)i / (float)SSAO_KERNEL_SIZE;
float scale = 0.1f + 0.9f * t * t;  /* lerp(0.1, 1.0, t^2) */
```

The samples are generated using `forge_hash_pcg()` from the math library for
deterministic pseudo-random numbers.

### Step 2: Create the noise texture (CPU, once at init)

A 4x4 texture of random rotation vectors tiles across the screen. Each texel
contains a unit-length vector in the XY plane that rotates the sampling kernel
differently at each pixel, breaking up the banding pattern that would appear
if every pixel used the same kernel orientation.

### Step 3: Per-pixel SSAO (fragment shader, every frame)

For each pixel:

1. **Reconstruct view-space position** from the depth buffer:

   ```hlsl
   float4 clip = float4(ndc_xy, depth, 1.0);
   float4 view = mul(inv_projection, clip);
   return view.xyz / view.w;
   ```

2. **Read the view-space normal** from the G-buffer.

3. **Build a TBN matrix** using Gram-Schmidt orthonormalization:

   ```hlsl
   float3 tangent   = normalize(random_vec - normal * dot(random_vec, normal));
   float3 bitangent = cross(normal, tangent);
   float3x3 TBN     = float3x3(tangent, bitangent, normal);
   ```

4. **For each of 64 kernel samples**, transform from tangent space to view
   space, offset from the fragment position, project to screen UV, sample the
   depth buffer, and check if the stored surface is closer than the sample
   (meaning it occludes the sample). A range check prevents distant geometry
   from contributing false occlusion.

5. **Output** `1.0 - (occluded / 64)` — white means fully lit, dark means
   occluded.

### Step 4: Box blur (4x4)

The raw SSAO output has a visible 4x4 tile pattern from the noise texture.
A 4x4 box blur averages exactly one noise tile, smoothing the pattern into a
clean AO factor.

### Step 5: Composite

The final pass multiplies the scene color by the AO factor. Three display modes
let you compare the effect:

- **Key 1** — AO only (white background with dark occlusion)
- **Key 2** — Full render with AO applied
- **Key 3** — Full render without AO (comparison)

## Controls

| Key | Action |
|-----|--------|
| **1** | AO only view (default) |
| **2** | Full render with AO |
| **3** | Full render without AO |
| **D** | Toggle IGN dithering/jitter |
| **WASD** | Move camera |
| **Space / LShift** | Move up / down |
| **Mouse** | Look around |
| **Escape** | Release mouse / quit |

## Key concepts

### Multiple render targets (MRT)

The geometry pass writes to two color attachments in a single draw call. The
fragment shader declares an output structure with two `SV_Target` semantics:

```hlsl
struct PSOutput {
    float4 color       : SV_Target0;  /* lit scene color       */
    float4 view_normal : SV_Target1;  /* view-space normal xyz */
};
```

On the CPU side, the pipeline is created with `num_color_targets = 2` and both
textures are attached when beginning the render pass.

### Depth reconstruction

Instead of storing world-space position in a G-buffer texture (expensive — 3
floats per pixel), we reconstruct the view-space position from the depth buffer
using the inverse projection matrix. This saves memory bandwidth at the cost of
a matrix multiply per SSAO sample.

### IGN dithering (Jimenez 2014)

Interleaved Gradient Noise adds a small per-pixel offset to the AO output,
breaking up 8-bit quantization banding in smooth gradients. The same technique
also jitters the kernel rotation angle for better sample distribution. Toggle
with **D** to see the difference.

## What's next

With SSAO adding contact occlusion, you have a solid deferred-style post-process
pipeline. From here you could explore:

- **Bilateral blur** to preserve edges (exercise 3 below)
- **Screen-space reflections (SSR)** using the same G-buffer
- **Temporal accumulation** to spread the SSAO cost over multiple frames

## AI skill

The [`ssao` skill](../../../.claude/skills/ssao/SKILL.md) can add SSAO to any
SDL3 GPU project — invoke it with `/ssao` in Claude Code.

## Exercises

1. **Adjust the radius** — change `SSAO_RADIUS` from 0.5 to 1.0 or 2.0 and
   observe how the occlusion spreads. Larger radii catch more geometry but lose
   fine detail.

2. **Reduce the kernel size** — try 16 or 32 samples instead of 64. Notice the
   increased noise and how the blur helps hide it.

3. **Try a bilateral blur** — replace the box blur with a bilateral filter that
   preserves edges by comparing depth/normal similarity between samples. This
   prevents the AO from bleeding across object boundaries.

4. **Power the AO** — in the composite shader, try `pow(ao, 2.0)` to increase
   the contrast of the occlusion effect.

5. **Half-resolution SSAO** — render the SSAO pass at half resolution and
   upsample for the composite. This roughly quarters the cost of the most
   expensive pass.

## Further reading

- [John Chapman — SSAO Tutorial](https://john-chapman-graphics.blogspot.com/2013/01/ssao-tutorial.html)
  — the hemisphere kernel approach used in this lesson
- [LearnOpenGL — SSAO](https://learnopengl.com/Advanced-Lighting/SSAO)
  — detailed walkthrough with OpenGL code
- Jimenez et al., "Next Generation Post Processing in Call of Duty: Advanced
  Warfare" (SIGGRAPH 2014) — IGN dithering technique
- Crytek, "Finding Next Gen" (SIGGRAPH 2007) — original SSAO paper

## Building

```bash
python scripts/compile_shaders.py 27       # compile shaders
cmake -B build
cmake --build build --config Debug --target 27-ssao
```

## Files

| File | Purpose |
|------|---------|
| `main.c` | Application: 5-pass renderer with SSAO |
| `shaders/shadow.vert.hlsl` | Transform vertices by light MVP |
| `shaders/shadow.frag.hlsl` | Empty (depth-only write) |
| `shaders/scene.vert.hlsl` | Transform to clip/world/view space |
| `shaders/scene.frag.hlsl` | Blinn-Phong + shadow; MRT output |
| `shaders/grid.vert.hlsl` | Transform grid quad |
| `shaders/grid.frag.hlsl` | Procedural grid + shadow; MRT output |
| `shaders/fullscreen.vert.hlsl` | SV_VertexID fullscreen quad |
| `shaders/ssao.frag.hlsl` | Hemisphere kernel SSAO |
| `shaders/blur.frag.hlsl` | 4x4 box blur |
| `shaders/composite.frag.hlsl` | Scene color * AO; mode switching |
