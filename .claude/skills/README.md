# Skills Index

All Claude Code skills available in this project. Skills are invoked with
`/skill-name` in chat or triggered automatically when relevant.

## Dev Skills — Workflow and Authoring

These skills manage the lesson development workflow: scaffolding, quality
checks, publishing, and workspace management.

| Skill | Description |
|---|---|
| `/dev-add-screenshot` | Capture a screenshot from a lesson executable and embed it in the README |
| `/dev-create-diagram` | Create a matplotlib diagram using the project's dark theme and visual identity |
| `/dev-create-lesson` | Add lesson content (README, skill file, screenshot) to an already-working GPU lesson |
| `/dev-create-pr` | Stage changes, create a branch, commit, and open a pull request |
| `/dev-engine-lesson` | Scaffold a new engine lesson (build systems, C, debugging, project structure) |
| `/dev-final-pass` | Run a systematic quality review on a lesson before publishing |
| `/dev-markdown-lint` | Check and fix markdown formatting issues with markdownlint-cli2 |
| `/dev-math-lesson` | Create a new math lesson and update the math library |
| `/dev-new-lesson` | Create a new lesson for the forge-gpu project |
| `/dev-publish-lesson` | Validate, commit, and create a PR for a new lesson |
| `/dev-reset-workspace` | Reset workspace to clean state on main (stop agents, clean branches) |
| `/dev-review-pr` | Check CI, review automated feedback, implement fixes, and merge a PR |
| `/dev-start-lesson` | Scaffold a new advanced GPU lesson directory with minimal main.c and PLAN.md |
| `/dev-ui-lesson` | Scaffold a new UI lesson (fonts, text, atlas, controls, layout) |
| `/dev-ui-review` | Run a UI-specific quality review before final pass |

## Forge Skills — GPU Techniques

These skills teach Claude how to implement specific GPU rendering techniques
using SDL3 GPU and the forge-gpu libraries. They are applied automatically
when building projects or can be invoked directly.

### Setup and Fundamentals

| Skill | Description |
|---|---|
| `forge-sdl-gpu-setup` | Set up an SDL3 GPU application with the callback architecture |
| `forge-first-triangle` | Draw colored geometry with vertex buffers, shaders, and a graphics pipeline |
| `forge-uniforms-and-motion` | Pass per-frame data to shaders with push uniforms |
| `forge-textures-and-samplers` | Load images, create GPU textures/samplers, draw textured geometry with index buffers |
| `forge-depth-and-3d` | Set up depth buffer, 3D MVP pipeline, back-face culling, and resize handling |
| `forge-camera-and-input` | First-person camera with quaternion orientation, keyboard/mouse input, delta time |

### Geometry and Loading

| Skill | Description |
|---|---|
| `forge-mesh-loading` | Load OBJ models, create textured meshes with mipmaps, fly-around camera |
| `forge-scene-loading` | Load glTF 2.0 scenes with multi-material meshes, hierarchy, and indexed drawing |
| `forge-instanced-rendering` | Draw many mesh copies efficiently with per-instance vertex buffers |
| `forge-mipmaps` | Mipmapped textures with auto-generated mip levels and sampler configuration |

### Lighting and Shadows

| Skill | Description |
|---|---|
| `forge-basic-lighting` | Blinn-Phong lighting (ambient + diffuse + specular) with world-space normals |
| `forge-blinn-phong-materials` | Per-material Blinn-Phong with ambient, diffuse, specular colors and shininess |
| `forge-normal-maps` | Tangent-space normal mapping with TBN matrix construction |
| `forge-cascaded-shadow-maps` | Cascaded shadow maps with PCF soft shadows for directional lights |
| `forge-point-light-shadows` | Omnidirectional point light shadows with cube map depth textures |
| `forge-gobo-spotlight` | Projected-texture (gobo/cookie) spotlight with cone falloff and shadow map |

### Post-Processing and Effects

| Skill | Description |
|---|---|
| `forge-hdr-tone-mapping` | HDR rendering with floating-point render targets and filmic tone mapping |
| `forge-bloom` | Jimenez dual-filter bloom with 13-tap downsample and Karis averaging |
| `forge-ssao` | Screen-space ambient occlusion with hemisphere-kernel sampling and blur |
| `forge-linear-fog` | Depth-based distance fog (linear, exponential, exponential-squared) |
| `forge-blending` | Alpha blending, alpha testing, and additive blending with blend state config |

### Environment and Reflections

| Skill | Description |
|---|---|
| `forge-environment-mapping` | Cube map environment mapping with skybox and reflective surfaces |
| `forge-planar-reflections` | Planar reflections with oblique near-plane clipping and Fresnel-blended water |
| `forge-screen-space-reflections` | Screen-space reflections (SSR) with ray marching |
| `forge-procedural-sky` | Physically-based atmospheric scattering (Rayleigh, Mie, ozone) with ray marching |

### Procedural and Compute

| Skill | Description |
|---|---|
| `forge-shader-grid` | Procedural anti-aliased grid floor with fwidth()/smoothstep() |
| `forge-shader-noise` | GPU noise functions (hash, value, Perlin, fBm, domain warping) |
| `forge-compute-shaders` | Compute pipeline with storage textures and compute-then-render pattern |

### Animation

| Skill | Description |
|---|---|
| `forge-transform-animations` | Keyframe animation with glTF loading, quaternion slerp, and path-following |

### Debug and Utilities

| Skill | Description |
|---|---|
| `forge-debug-lines` | Immediate-mode debug line drawing (grids, axes, circles, wireframe boxes) |

### UI

| Skill | Description |
|---|---|
| `forge-ui-rendering` | Render immediate-mode UI with single draw call, font atlas, and alpha blending |
| `forge-auto-widget-layout` | Stack-based automatic widget layout with padding, spacing, and nesting |
| `forge-draggable-windows` | Draggable, z-ordered, collapsible windows with deferred draw ordering |
