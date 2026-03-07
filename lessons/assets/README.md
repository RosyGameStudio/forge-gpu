# Asset Pipeline Lessons

A hybrid Python + C track for building asset processing tooling — from a CLI
pipeline to a full asset editor with a web frontend.

## Purpose

Asset pipeline lessons teach how to build tooling that transforms raw art
(textures, meshes, scenes) into optimized, GPU-ready formats:

- Scaffold a plugin-based CLI pipeline with TOML configuration (Python)
- Process textures: resize, compress, generate mipmaps, track changes (Python)
- Process meshes: deduplicate vertices, optimize indices, generate tangents and
  LODs using meshoptimizer and MikkTSpace (C tool invoked from Python)
- Generate procedural geometry: spheres, tori, capsules, and more from
  parametric equations (C header-only library)
- Pack processed assets into compressed bundles with random access
- Build a web frontend for browsing, previewing, and configuring assets
- Create a visual scene editor with undo/redo and live preview

## Philosophy

- **Python orchestrates, C processes** — The pipeline CLI is Python for rapid
  development and plugin flexibility. Performance-critical processing (mesh
  optimization, tangent generation) uses compiled C tools invoked as
  subprocesses. Procedural geometry lives in a header-only C library that GPU
  lessons include directly.
- **Incremental builds** — Only reprocess what changed. Fingerprint source
  files and skip unchanged assets.
- **Plugin architecture** — Each asset type (texture, mesh, scene) is a plugin
  that registers with the pipeline. New formats are added without modifying
  core code.
- **Metadata first** — Every processed asset has a sidecar file describing its
  source, settings, and output. The pipeline is fully reproducible.
- **Web-native tooling** — The frontend is a browser application served by
  an embedded Python server. No Electron, no native UI.

## Lessons

| # | Topic | Language | What you'll learn |
|---|-------|----------|-------------------|
| | *Coming soon* | | See [PLAN.md](../../PLAN.md) for the roadmap |

## Prerequisites

- Python 3.10+ (for pipeline CLI and web frontend)
- CMake 3.24+ and a C compiler (for mesh processing tools and procedural
  geometry library)
- pip (for installing Python dependencies per lesson)

Individual lessons may add dependencies (Pillow, Flask/FastAPI, meshoptimizer,
MikkTSpace) as needed — each lesson's README lists its requirements.

## Running

```bash
# Python lessons — from a lesson directory
pip install -r requirements.txt   # if present
python -m pipeline                # or as documented per lesson

# C tools — built via CMake alongside GPU lessons
cmake -B build
cmake --build build --config Debug
```

## Connection to other tracks

Asset pipeline lessons produce optimized assets consumed by GPU lessons. The
mesh processing plugin outputs the same binary format that `forge_gltf_load()`
and `forge_obj_load()` parse, and texture processing generates mipmapped
compressed textures ready for `SDL_CreateGPUTexture`. The procedural geometry
library (`common/shapes/`) is used directly by GPU and physics lessons.

| Track | Connection |
|---|---|
| GPU | Pipeline outputs feed directly into GPU lesson rendering; procedural geometry provides shapes for scenes |
| Engine | Build system concepts (incremental builds, dependency tracking) overlap; C tool compilation uses CMake |
| Math | Parametric surface equations build on vectors and trigonometry from math fundamentals |
| Physics | Procedural shapes (spheres, capsules, boxes) serve as collision proxies |
