---
name: shader-grid
description: >
  Add a procedural anti-aliased grid floor to an SDL3 GPU scene using
  fwidth()/smoothstep() in the fragment shader. Covers procedural rendering,
  screen-space derivatives for anti-aliasing, distance fade, and using multiple
  graphics pipelines in a single render pass.
---

# Shader Grid

## When to use

- You need a **ground grid** or floor plane in a 3D scene
- You want **procedural patterns** (no texture) with anti-aliasing
- You need a **debug visualization** plane for positioning objects
- You want to combine multiple rendering techniques in one render pass
- You need to render objects with **different vertex formats** in the same pass

## Core technique: anti-aliased procedural grid

The grid is rendered on a flat quad. The fragment shader computes grid lines
procedurally from the world-space position using screen-space derivatives.

### HLSL fragment shader pattern

```hlsl
/* Step 1: Scale world position to grid space */
float2 grid_uv = world_pos.xz / grid_spacing;

/* Step 2: Distance to nearest grid line (0 = on line, 0.5 = between) */
float2 dist = abs(frac(grid_uv - 0.5) - 0.5);

/* Step 3: Screen-space rate of change (pixel footprint in grid space) */
float2 fw = fwidth(grid_uv);

/* Step 4: Anti-aliased line mask (smooth edge, not hard step) */
float2 aa_line = 1.0 - smoothstep(line_width, line_width + fw, dist);

/* Step 5: Combine X and Z lines */
float grid = max(aa_line.x, aa_line.y);

/* Step 6: Distance fade (prevent far-field moire) */
float cam_dist = length(world_pos - eye_pos.xyz);
float fade = 1.0 - smoothstep(fade_distance * 0.5, fade_distance, cam_dist);
grid *= fade;

/* Mix line and background colors */
float3 surface = lerp(bg_color.rgb, line_color.rgb, grid);
```

### Why this works

- `fwidth()` uses the same derivative hardware as mip selection (Lesson 05)
- The transition width is always one pixel, so lines look crisp at any distance
- Distance fade removes moire when grid cells become sub-pixel

## Multiple pipelines pattern

Different objects in a scene often need different pipeline configurations.
You can switch pipelines within a single render pass:

```c
/* Begin one render pass with color + depth targets */
SDL_GPURenderPass *pass = SDL_BeginGPURenderPass(cmd, &color, 1, &depth);

/* Pipeline 1: grid (position-only vertices, no sampler, CULL_NONE) */
SDL_BindGPUGraphicsPipeline(pass, grid_pipeline);
SDL_PushGPUVertexUniformData(cmd, 0, &grid_vert_uniforms, sizeof(...));
SDL_PushGPUFragmentUniformData(cmd, 0, &grid_frag_uniforms, sizeof(...));
/* bind grid VB/IB, draw */

/* Pipeline 2: model (pos+normal+uv vertices, sampler, CULL_BACK) */
SDL_BindGPUGraphicsPipeline(pass, model_pipeline);
/* push model uniforms, bind model VB/IB, draw */

SDL_EndGPURenderPass(pass);
```

Both pipelines share the same render targets. The depth buffer handles
occlusion between them automatically.

## Grid pipeline setup

```c
/* Vertex format: position only (float3, 12 bytes) */
SDL_GPUVertexAttribute grid_attr;
SDL_zero(grid_attr);
grid_attr.location = 0;
grid_attr.format   = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
grid_attr.offset   = 0;

/* Pipeline: no backface culling (visible from both sides) */
pipeline_info.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;

/* Fragment shader: 0 samplers, 1 uniform buffer */
/* Vertex shader: 0 samplers, 1 uniform buffer (VP matrix) */
```

## Grid geometry

A simple quad on the XZ plane:

```c
#define GRID_HALF_SIZE 50.0f

float vertices[] = {
    -GRID_HALF_SIZE, 0.0f, -GRID_HALF_SIZE,
     GRID_HALF_SIZE, 0.0f, -GRID_HALF_SIZE,
     GRID_HALF_SIZE, 0.0f,  GRID_HALF_SIZE,
    -GRID_HALF_SIZE, 0.0f,  GRID_HALF_SIZE,
};
Uint16 indices[] = { 0, 1, 2, 0, 2, 3 };
```

## Grid fragment uniforms (96 bytes)

```c
typedef struct GridFragUniforms {
    float line_color[4];    /* grid line color (RGBA, linear space)    */
    float bg_color[4];      /* background color (RGBA, linear space)   */
    float light_dir[4];     /* light direction (xyz, w unused)         */
    float eye_pos[4];       /* camera position (xyz, w unused)         */
    float grid_spacing;     /* world units between lines (e.g. 1.0)   */
    float line_width;       /* line thickness in grid space (e.g. 0.02)*/
    float fade_distance;    /* distance for fade-out (e.g. 40.0)      */
    float ambient;          /* ambient intensity [0..1]                */
    float shininess;        /* specular exponent                       */
    float specular_str;     /* specular intensity [0..1]               */
    float _pad0, _pad1;     /* pad to 16-byte alignment                */
} GridFragUniforms;
```

## Common mistakes

1. **Forgetting `fwidth()`** — without it, lines alias badly at distance
2. **No distance fade** — distant grid creates moire/shimmer at the horizon
3. **Using `frac()` directly** — `frac(grid_uv)` puts the discontinuity at
   the grid line; `frac(grid_uv - 0.5) - 0.5` centers the smooth region on
   the line, which is what you want
4. **Wrong color space** — with SDR_LINEAR swapchain, all colors must be in
   linear space (not sRGB). Convert hex colors: `(value/255)^2.2`
5. **Missing depth write on grid** — the grid must write to the depth buffer
   for correct occlusion with other objects
6. **Culling the grid** — use `CULL_NONE` so the grid is visible from below too

## Reference

- **Lesson**: [GPU Lesson 12 — Shader Grid](../../../lessons/gpu/12-shader-grid/)
- **Math**: screen-space derivatives (Lesson 05), Blinn-Phong (Lesson 10)
- **Concept**: `fwidth()` = `abs(ddx()) + abs(ddy())` — pixel footprint
