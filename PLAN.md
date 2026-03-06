# forge-gpu — Lesson Plan

## Completed

All foundation, tooling, and the following lesson tracks are complete:

- **Foundation** — Project scaffolding, math library, test suite, skills
- **GPU Lessons 01–33** — From Hello Window through Vertex Pulling
- **Math Lessons 01–15** — From Vectors through Bezier Curves
- **Engine Lessons 01–11** — From Intro to C through Git & Version Control
- **UI Lessons 01–13** — From TTF Parsing through Theming and Color System
- **Developer tooling** — Run script, shader compilation, setup script, screenshot capture

## GPU Lessons — Remaining

### Advanced Rendering

- [ ] **Lesson 34 — Indirect Drawing** — GPU-driven draw calls with `SDL_DrawGPUPrimitivesIndirect` / `SDL_DrawGPUIndexedPrimitivesIndirect`; filling indirect argument buffers from compute shaders; basic GPU culling (frustum cull in compute, emit surviving draws); reducing CPU draw-call overhead
- [ ] **Lesson 35 — Particle Animations** — Billboard quad particles facing the camera; GPU particle buffer updated via compute shader; spawn, simulate (gravity, drag, lifetime), and render loop; atlas-based animated particles; additive and soft-particle blending (depends on Lessons 11 and 16)
- [ ] **Lesson 36 — Imposters** — Billboard LOD representations of complex meshes; baking an imposter atlas (multiple view angles); selecting the correct atlas frame based on view direction; cross-fading between imposter and full mesh; application to distant trees, props, and crowd rendering

### Advanced Materials & Effects

- [ ] **Lesson 37 — Translucent Materials** — Approximating light transmission through thin and thick surfaces; wrap lighting for subsurface scattering approximation; thickness maps; back-face lighting contribution; application to foliage, wax, skin, and fabric
- [ ] **Lesson 38 — Water Caustics** — Projecting animated caustic patterns onto underwater surfaces; caustic texture animation (scrolling, distortion); light attenuation with water depth; combining with existing lighting and shadow systems
- [ ] **Lesson 39 — IBL with Probes** — Image-based lighting using irradiance maps (diffuse) and pre-filtered environment maps (specular); split-sum approximation with a BRDF LUT; placing reflection probes in a scene; blending between probes; integrating IBL as ambient lighting replacement

### Volumetric & Terrain

- [ ] **Lesson 40 — Volumetric Fog** — Ray marching through participating media in a froxel grid or screen-space pass; Beer-Lambert absorption; in-scattering from lights with shadow map sampling; temporal reprojection for performance; combining volumetric fog with scene rendering
- [ ] **Lesson 41 — Grass with Animations & Imposters** — Dense grass field rendering; geometry instancing or compute-generated grass blades; wind animation using noise-based displacement; LOD transition from full blades to imposter cards at distance; terrain integration (depends on Lessons 13, 25, 35)
- [ ] **Lesson 42 — Height Map Terrain** — GPU terrain from height map; LOD with distance-based tessellation or geo-clipmaps; normal computation from height samples; texture splatting with blend maps; integrating with grass rendering

## UI Lessons — Remaining

### Application Patterns

- [ ] **UI Lesson 14 — Game UI** — Health bars, inventories, HUD elements, menus; game-specific patterns using the immediate-mode controls from earlier lessons; fixed and proportional layout for different screen sizes
- [ ] **UI Lesson 15 — Dev UI** — Property editors, debug overlays, console, performance graphs; developer-facing tools for inspecting game state; collapsible sections and tree views
