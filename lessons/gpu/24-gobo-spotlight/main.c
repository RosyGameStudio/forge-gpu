/*
 * GPU Lesson 24 — Gobo Spotlight
 *
 * Projected-texture (cookie/gobo) spotlight with inner/outer cone angles,
 * smooth falloff, gobo pattern projection, and shadow mapping from the
 * spotlight's frustum.
 *
 * Scene: CesiumMilkTruck + crates on a procedural grid floor, lit by a
 * theatrical spotlight projecting a gobo pattern. A low-poly searchlight
 * model marks the light source position.
 *
 * Controls:
 *   WASD / Space / LShift — Move camera
 *   Mouse                 — Look around
 *   Escape                — Release mouse / quit
 *
 * SPDX-License-Identifier: Zlib
 */
#define SDL_MAIN_USE_CALLBACKS 1

#include "gltf/forge_gltf.h"
#include "math/forge_math.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <stddef.h> /* offsetof */

/* ── Compiled shader bytecodes ──────────────────────────────────────────── */

#include "shaders/compiled/grid_frag_dxil.h"
#include "shaders/compiled/grid_frag_spirv.h"
#include "shaders/compiled/grid_vert_dxil.h"
#include "shaders/compiled/grid_vert_spirv.h"

#include "shaders/compiled/scene_frag_dxil.h"
#include "shaders/compiled/scene_frag_spirv.h"
#include "shaders/compiled/scene_vert_dxil.h"
#include "shaders/compiled/scene_vert_spirv.h"

#include "shaders/compiled/shadow_frag_dxil.h"
#include "shaders/compiled/shadow_frag_spirv.h"
#include "shaders/compiled/shadow_vert_dxil.h"
#include "shaders/compiled/shadow_vert_spirv.h"

/* ── Constants ──────────────────────────────────────────────────────────── */

#define WINDOW_WIDTH  1280
#define WINDOW_HEIGHT 720

/* Camera. */
#define FOV_DEG            60
#define NEAR_PLANE         0.1f
#define FAR_PLANE          100.0f
#define CAM_SPEED          5.0f
#define MOUSE_SENS         0.003f
#define PITCH_CLAMP        1.5f

/* Camera initial position — elevated, looking at the scene. */
#define CAM_START_X         5.0f
#define CAM_START_Y         4.0f
#define CAM_START_Z         8.0f
#define CAM_START_YAW_DEG   34.0f
#define CAM_START_PITCH_DEG -20.0f

/* Scene material defaults. */
#define MATERIAL_AMBIENT      0.01f
#define MATERIAL_SHININESS    64.0f
#define MATERIAL_SPECULAR_STR 0.5f

/* Dim directional fill light — just enough to show surface detail.
 * Points down and to the right (like a weak overhead fill). */
#define FILL_INTENSITY  0.03f
#define FILL_DIR_X      0.3f
#define FILL_DIR_Y     -0.8f
#define FILL_DIR_Z      0.2f

/* Spotlight — position, direction, cone angles, and color. */
#define SPOT_POS_X       6.0f
#define SPOT_POS_Y       5.0f
#define SPOT_POS_Z       4.0f
#define SPOT_TARGET_X    0.0f
#define SPOT_TARGET_Y    0.0f
#define SPOT_TARGET_Z    0.0f
#define SPOT_INNER_DEG   20.0f   /* full-intensity inner cone half-angle */
#define SPOT_OUTER_DEG   30.0f   /* falloff-to-zero outer cone half-angle */
#define SPOT_INTENSITY   5.0f    /* HDR brightness */
#define SPOT_COLOR_R     1.0f    /* warm white spotlight */
#define SPOT_COLOR_G     0.95f
#define SPOT_COLOR_B     0.8f
#define SPOT_NEAR        0.5f
#define SPOT_FAR         30.0f

/* Shadow map. */
#define SHADOW_MAP_SIZE   1024
#define SHADOW_DEPTH_FMT  SDL_GPU_TEXTUREFORMAT_D32_FLOAT

/* Gobo texture path (relative to executable). */
#define GOBO_TEXTURE_PATH "assets/gobo_window.png"

/* Grid. */
#define GRID_HALF_SIZE     50.0f
#define GRID_INDEX_COUNT   6
#define GRID_SPACING       1.0f
#define GRID_LINE_WIDTH    0.02f
#define GRID_FADE_DISTANCE 40.0f

/* Grid colors (linear space). */
#define GRID_LINE_R 0.15f
#define GRID_LINE_G 0.55f
#define GRID_LINE_B 0.85f
#define GRID_BG_R   0.04f
#define GRID_BG_G   0.04f
#define GRID_BG_B   0.08f

/* Clear color — dark background. */
#define CLEAR_R 0.008f
#define CLEAR_G 0.008f
#define CLEAR_B 0.026f

/* Frame timing. */
#define MAX_FRAME_DT 0.1f

/* Model asset paths (relative to executable). */
#define TRUCK_MODEL_PATH       "assets/models/CesiumMilkTruck/CesiumMilkTruck.gltf"
#define BOX_MODEL_PATH         "assets/models/BoxTextured/BoxTextured.gltf"
#define SEARCHLIGHT_MODEL_PATH "assets/models/Searchlight/scene.gltf"

/* Box placement — a few crates scattered for the spotlight to illuminate. */
#define BOX_COUNT 5

/* Searchlight placement — the Sketchfab model has a 100x scale baked in,
 * so we counter-scale it to fit the scene (~1 unit tall). */
#define SEARCHLIGHT_SCALE 0.003f

#define BYTES_PER_PIXEL 4

/* Texture sampler — trilinear filtering with anisotropy. */
#define MAX_ANISOTROPY 4

/* ── Uniform structures ─────────────────────────────────────────────────── */

/* Scene vertex uniforms — pushed per draw call. */
typedef struct SceneVertUniforms {
    mat4 mvp;   /* model-view-projection matrix (64 bytes) */
    mat4 model; /* model (world) matrix         (64 bytes) */
} SceneVertUniforms;

/* Scene fragment uniforms — matches scene.frag.hlsl cbuffer. */
typedef struct SceneFragUniforms {
    float base_color[4];    /* material RGBA               (16 bytes) */
    float eye_pos[3];       /* camera position              (12 bytes) */
    float has_texture;      /* > 0.5 = sample diffuse_tex    (4 bytes) */
    float ambient;          /* ambient intensity              (4 bytes) */
    float fill_intensity;   /* directional fill strength      (4 bytes) */
    float shininess;        /* specular exponent              (4 bytes) */
    float specular_str;     /* specular strength              (4 bytes) */
    float fill_dir[4];      /* fill light direction (xyz,pad) (16 bytes) */
    float spot_pos[3];      /* spotlight world position       (12 bytes) */
    float spot_intensity;   /* spotlight HDR brightness        (4 bytes) */
    float spot_dir[3];      /* spotlight direction (unit)     (12 bytes) */
    float cos_inner;        /* cos(inner cone half-angle)      (4 bytes) */
    float spot_color[3];    /* spotlight RGB color            (12 bytes) */
    float cos_outer;        /* cos(outer cone half-angle)      (4 bytes) */
    mat4  light_vp;         /* spotlight view-projection      (64 bytes) */
} SceneFragUniforms;        /* 192 bytes total */

/* Shadow vertex uniforms — just the light MVP per draw call. */
typedef struct ShadowVertUniforms {
    mat4 light_mvp; /* light VP * model matrix (64 bytes) */
} ShadowVertUniforms;

/* Grid vertex uniforms — one VP matrix. */
typedef struct GridVertUniforms {
    mat4 vp;  /* view-projection matrix (64 bytes) */
} GridVertUniforms;

/* Grid fragment uniforms — matches grid.frag.hlsl cbuffer. */
typedef struct GridFragUniforms {
    float line_color[4]; /* grid line color            (16 bytes) */
    float bg_color[4];   /* background color           (16 bytes) */
    float eye_pos[3];    /* camera position            (12 bytes) */
    float grid_spacing;  /* world units / line          (4 bytes) */
    float line_width;    /* line thickness              (4 bytes) */
    float fade_distance; /* fade-out distance           (4 bytes) */
    float _pad0;         /*                             (4 bytes) */
    float _pad1;         /*                             (4 bytes) */
    float spot_pos[3];   /* spotlight world position   (12 bytes) */
    float spot_intensity;/* spotlight HDR brightness     (4 bytes) */
    float spot_dir[3];   /* spotlight direction (unit)  (12 bytes) */
    float cos_inner;     /* cos(inner cone half-angle)   (4 bytes) */
    float spot_color[3]; /* spotlight RGB color         (12 bytes) */
    float cos_outer;     /* cos(outer cone half-angle)   (4 bytes) */
    mat4  light_vp;      /* spotlight view-projection   (64 bytes) */
} GridFragUniforms;      /* 192 bytes total */

/* ── GPU-side model types ────────────────────────────────────────────────── */

typedef struct GpuPrimitive {
    SDL_GPUBuffer *vertex_buffer;
    SDL_GPUBuffer *index_buffer;
    Uint32 index_count;
    int material_index;
    SDL_GPUIndexElementSize index_type;
    bool has_uvs;
} GpuPrimitive;

typedef struct GpuMaterial {
    float base_color[4];
    SDL_GPUTexture *texture;
    bool has_texture;
} GpuMaterial;

typedef struct ModelData {
    ForgeGltfScene scene;
    GpuPrimitive  *primitives;
    int            primitive_count;
    GpuMaterial   *materials;
    int            material_count;
} ModelData;

typedef struct BoxPlacement {
    vec3  position;
    float y_rotation;
} BoxPlacement;

/* ── Application state ──────────────────────────────────────────────────── */

typedef struct app_state {
    SDL_Window    *window;
    SDL_GPUDevice *device;

    /* Pipelines. */
    SDL_GPUGraphicsPipeline *scene_pipeline;
    SDL_GPUGraphicsPipeline *grid_pipeline;
    SDL_GPUGraphicsPipeline *shadow_pipeline;

    /* Depth buffer (main render pass). */
    SDL_GPUTexture *depth_texture;
    Uint32 depth_width;
    Uint32 depth_height;

    /* Grid geometry. */
    SDL_GPUBuffer *grid_vertex_buffer;
    SDL_GPUBuffer *grid_index_buffer;

    /* Textures and samplers. */
    SDL_GPUTexture *white_texture;
    SDL_GPUSampler *sampler;           /* trilinear for diffuse textures */

    /* Shadow map — single 2D depth texture from the spotlight's frustum. */
    SDL_GPUTexture *shadow_depth_texture;
    SDL_GPUSampler *shadow_sampler;    /* nearest, clamp-to-edge */

    /* Gobo pattern — grayscale texture projected through the spotlight. */
    SDL_GPUTexture *gobo_texture;
    SDL_GPUSampler *gobo_sampler;      /* linear, clamp-to-edge */

    /* Spotlight view-projection matrix (static — light doesn't move). */
    mat4 light_vp;
    vec3 spot_dir;                     /* normalized spotlight direction */

    /* Models. */
    ModelData truck;
    ModelData box;
    ModelData searchlight;
    BoxPlacement box_placements[BOX_COUNT];

    /* Searchlight placement matrix. */
    mat4 searchlight_placement;

    /* Swapchain format (queried after setting SDR_LINEAR). */
    SDL_GPUTextureFormat swapchain_format;

    /* Camera. */
    vec3  cam_position;
    float cam_yaw;
    float cam_pitch;

    /* Timing and input. */
    Uint64 last_ticks;
    bool   mouse_captured;
} app_state;

/* ── Helper: create shader from embedded bytecode ───────────────────────── */

static SDL_GPUShader *create_shader(
    SDL_GPUDevice *device,
    SDL_GPUShaderStage stage,
    const Uint8 *spirv_code, size_t spirv_size,
    const Uint8 *dxil_code,  size_t dxil_size,
    Uint32 num_samplers,
    Uint32 num_uniform_buffers)
{
    SDL_GPUShaderFormat formats = SDL_GetGPUShaderFormats(device);

    SDL_GPUShaderCreateInfo info;
    SDL_zero(info);
    info.stage              = stage;
    info.entrypoint         = "main";
    info.num_samplers       = num_samplers;
    info.num_uniform_buffers = num_uniform_buffers;

    if (formats & SDL_GPU_SHADERFORMAT_SPIRV) {
        info.format    = SDL_GPU_SHADERFORMAT_SPIRV;
        info.code      = spirv_code;
        info.code_size = spirv_size;
    } else if (formats & SDL_GPU_SHADERFORMAT_DXIL) {
        info.format    = SDL_GPU_SHADERFORMAT_DXIL;
        info.code      = dxil_code;
        info.code_size = dxil_size;
    } else {
        SDL_Log("No supported shader format available");
        return NULL;
    }

    SDL_GPUShader *shader = SDL_CreateGPUShader(device, &info);
    if (!shader) {
        SDL_Log("Failed to create shader: %s", SDL_GetError());
    }
    return shader;
}

/* ── Helper: upload buffer data ─────────────────────────────────────────── */

static SDL_GPUBuffer *upload_gpu_buffer(
    SDL_GPUDevice *device, SDL_GPUBufferUsageFlags usage,
    const void *data, Uint32 size)
{
    SDL_GPUBufferCreateInfo buf_info;
    SDL_zero(buf_info);
    buf_info.usage = usage;
    buf_info.size  = size;

    SDL_GPUBuffer *buffer = SDL_CreateGPUBuffer(device, &buf_info);
    if (!buffer) {
        SDL_Log("Failed to create GPU buffer: %s", SDL_GetError());
        return NULL;
    }

    SDL_GPUTransferBufferCreateInfo xfer_info;
    SDL_zero(xfer_info);
    xfer_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    xfer_info.size  = size;

    SDL_GPUTransferBuffer *xfer = SDL_CreateGPUTransferBuffer(device, &xfer_info);
    if (!xfer) {
        SDL_Log("Failed to create transfer buffer: %s", SDL_GetError());
        SDL_ReleaseGPUBuffer(device, buffer);
        return NULL;
    }

    void *mapped = SDL_MapGPUTransferBuffer(device, xfer, false);
    if (!mapped) {
        SDL_Log("Failed to map transfer buffer: %s", SDL_GetError());
        SDL_ReleaseGPUTransferBuffer(device, xfer);
        SDL_ReleaseGPUBuffer(device, buffer);
        return NULL;
    }
    SDL_memcpy(mapped, data, size);
    SDL_UnmapGPUTransferBuffer(device, xfer);

    SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(device);
    if (!cmd) {
        SDL_Log("Failed to acquire command buffer for upload: %s", SDL_GetError());
        SDL_ReleaseGPUTransferBuffer(device, xfer);
        SDL_ReleaseGPUBuffer(device, buffer);
        return NULL;
    }

    SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(cmd);

    SDL_GPUTransferBufferLocation src;
    SDL_zero(src);
    src.transfer_buffer = xfer;

    SDL_GPUBufferRegion dst;
    SDL_zero(dst);
    dst.buffer = buffer;
    dst.size   = size;

    SDL_UploadToGPUBuffer(copy, &src, &dst, false);
    SDL_EndGPUCopyPass(copy);

    if (!SDL_SubmitGPUCommandBuffer(cmd)) {
        SDL_Log("Failed to submit upload command buffer: %s", SDL_GetError());
    }

    SDL_ReleaseGPUTransferBuffer(device, xfer);
    return buffer;
}

/* ── Helper: load texture from file ─────────────────────────────────────── */

static SDL_GPUTexture *load_texture(SDL_GPUDevice *device, const char *path)
{
    SDL_Surface *surface = SDL_LoadSurface(path);
    if (!surface) {
        SDL_Log("Failed to load texture '%s': %s", path, SDL_GetError());
        return NULL;
    }

    SDL_Surface *converted = SDL_ConvertSurface(surface, SDL_PIXELFORMAT_ABGR8888);
    SDL_DestroySurface(surface);
    if (!converted) {
        SDL_Log("Failed to convert surface: %s", SDL_GetError());
        return NULL;
    }

    Uint32 w = (Uint32)converted->w;
    Uint32 h = (Uint32)converted->h;
    Uint32 max_dim = w > h ? w : h;
    Uint32 mip_levels = (Uint32)(forge_log2f((float)max_dim)) + 1;

    SDL_GPUTextureCreateInfo tex_info;
    SDL_zero(tex_info);
    tex_info.type                = SDL_GPU_TEXTURETYPE_2D;
    tex_info.format              = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB;
    tex_info.width               = w;
    tex_info.height              = h;
    tex_info.layer_count_or_depth = 1;
    tex_info.num_levels          = mip_levels;
    tex_info.usage               = SDL_GPU_TEXTUREUSAGE_SAMPLER |
                                   SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;

    SDL_GPUTexture *tex = SDL_CreateGPUTexture(device, &tex_info);
    if (!tex) {
        SDL_Log("Failed to create texture: %s", SDL_GetError());
        SDL_DestroySurface(converted);
        return NULL;
    }

    Uint32 row_bytes   = w * BYTES_PER_PIXEL;
    Uint32 total_bytes = w * h * BYTES_PER_PIXEL;

    SDL_GPUTransferBufferCreateInfo xfer_info;
    SDL_zero(xfer_info);
    xfer_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    xfer_info.size  = total_bytes;

    SDL_GPUTransferBuffer *xfer = SDL_CreateGPUTransferBuffer(device, &xfer_info);
    if (!xfer) {
        SDL_Log("Failed to create texture transfer buffer: %s", SDL_GetError());
        SDL_ReleaseGPUTexture(device, tex);
        SDL_DestroySurface(converted);
        return NULL;
    }

    void *mapped = SDL_MapGPUTransferBuffer(device, xfer, false);
    if (!mapped) {
        SDL_Log("Failed to map texture transfer buffer: %s", SDL_GetError());
        SDL_ReleaseGPUTransferBuffer(device, xfer);
        SDL_ReleaseGPUTexture(device, tex);
        SDL_DestroySurface(converted);
        return NULL;
    }

    /* Copy row by row (surface pitch may differ from dest row bytes). */
    {
        const Uint8 *row_src = (const Uint8 *)converted->pixels;
        Uint8 *row_dst = (Uint8 *)mapped;
        for (Uint32 row = 0; row < h; row++) {
            SDL_memcpy(row_dst + row * row_bytes,
                       row_src + row * converted->pitch, row_bytes);
        }
    }
    SDL_UnmapGPUTransferBuffer(device, xfer);
    SDL_DestroySurface(converted);

    SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(device);
    if (!cmd) {
        SDL_Log("Failed to acquire cmd for texture upload: %s", SDL_GetError());
        SDL_ReleaseGPUTransferBuffer(device, xfer);
        SDL_ReleaseGPUTexture(device, tex);
        return NULL;
    }

    SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(cmd);

    SDL_GPUTextureTransferInfo src;
    SDL_zero(src);
    src.transfer_buffer = xfer;

    SDL_GPUTextureRegion dst;
    SDL_zero(dst);
    dst.texture = tex;
    dst.w       = w;
    dst.h       = h;
    dst.d       = 1;

    SDL_UploadToGPUTexture(copy, &src, &dst, false);
    SDL_EndGPUCopyPass(copy);
    SDL_GenerateMipmapsForGPUTexture(cmd, tex);

    if (!SDL_SubmitGPUCommandBuffer(cmd)) {
        SDL_Log("Failed to submit texture upload: %s", SDL_GetError());
        SDL_ReleaseGPUTransferBuffer(device, xfer);
        SDL_ReleaseGPUTexture(device, tex);
        return NULL;
    }

    SDL_ReleaseGPUTransferBuffer(device, xfer);
    return tex;
}

/* ── Helper: 1x1 white placeholder texture ──────────────────────────────── */

static SDL_GPUTexture *create_white_texture(SDL_GPUDevice *device)
{
    SDL_GPUTextureCreateInfo tex_info;
    SDL_zero(tex_info);
    tex_info.type                = SDL_GPU_TEXTURETYPE_2D;
    tex_info.format              = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB;
    tex_info.width               = 1;
    tex_info.height              = 1;
    tex_info.layer_count_or_depth = 1;
    tex_info.num_levels          = 1;
    tex_info.usage               = SDL_GPU_TEXTUREUSAGE_SAMPLER;

    SDL_GPUTexture *tex = SDL_CreateGPUTexture(device, &tex_info);
    if (!tex) {
        SDL_Log("Failed to create white texture: %s", SDL_GetError());
        return NULL;
    }

    Uint8 white[4] = { 255, 255, 255, 255 };

    SDL_GPUTransferBufferCreateInfo xfer_info;
    SDL_zero(xfer_info);
    xfer_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    xfer_info.size  = sizeof(white);

    SDL_GPUTransferBuffer *xfer = SDL_CreateGPUTransferBuffer(device, &xfer_info);
    if (!xfer) {
        SDL_Log("Failed to create white texture xfer: %s", SDL_GetError());
        SDL_ReleaseGPUTexture(device, tex);
        return NULL;
    }

    void *mapped = SDL_MapGPUTransferBuffer(device, xfer, false);
    if (!mapped) {
        SDL_Log("Failed to map white texture xfer: %s", SDL_GetError());
        SDL_ReleaseGPUTransferBuffer(device, xfer);
        SDL_ReleaseGPUTexture(device, tex);
        return NULL;
    }
    SDL_memcpy(mapped, white, sizeof(white));
    SDL_UnmapGPUTransferBuffer(device, xfer);

    SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(device);
    if (!cmd) {
        SDL_Log("Failed to acquire cmd for white texture: %s", SDL_GetError());
        SDL_ReleaseGPUTransferBuffer(device, xfer);
        SDL_ReleaseGPUTexture(device, tex);
        return NULL;
    }

    SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(cmd);

    SDL_GPUTextureTransferInfo src;
    SDL_zero(src);
    src.transfer_buffer = xfer;

    SDL_GPUTextureRegion dst;
    SDL_zero(dst);
    dst.texture = tex;
    dst.w       = 1;
    dst.h       = 1;
    dst.d       = 1;

    SDL_UploadToGPUTexture(copy, &src, &dst, false);
    SDL_EndGPUCopyPass(copy);

    if (!SDL_SubmitGPUCommandBuffer(cmd)) {
        SDL_Log("Failed to submit white texture upload: %s", SDL_GetError());
        SDL_ReleaseGPUTransferBuffer(device, xfer);
        SDL_ReleaseGPUTexture(device, tex);
        return NULL;
    }

    SDL_ReleaseGPUTransferBuffer(device, xfer);
    return tex;
}

/* ── Helper: load gobo texture (linear UNORM, no mipmaps) ──────────────── */

static SDL_GPUTexture *load_gobo_texture(SDL_GPUDevice *device, const char *path)
{
    SDL_Surface *surface = SDL_LoadSurface(path);
    if (!surface) {
        SDL_Log("Failed to load gobo texture '%s': %s", path, SDL_GetError());
        return NULL;
    }

    SDL_Surface *converted = SDL_ConvertSurface(surface, SDL_PIXELFORMAT_ABGR8888);
    SDL_DestroySurface(surface);
    if (!converted) {
        SDL_Log("Failed to convert gobo surface: %s", SDL_GetError());
        return NULL;
    }

    Uint32 w = (Uint32)converted->w;
    Uint32 h = (Uint32)converted->h;

    /* Use UNORM (not sRGB) — the gobo is a linear light attenuation mask,
     * not a color texture.  We sample .r in the shader. */
    SDL_GPUTextureCreateInfo tex_info;
    SDL_zero(tex_info);
    tex_info.type                = SDL_GPU_TEXTURETYPE_2D;
    tex_info.format              = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    tex_info.width               = w;
    tex_info.height              = h;
    tex_info.layer_count_or_depth = 1;
    tex_info.num_levels          = 1;
    tex_info.usage               = SDL_GPU_TEXTUREUSAGE_SAMPLER;

    SDL_GPUTexture *tex = SDL_CreateGPUTexture(device, &tex_info);
    if (!tex) {
        SDL_Log("Failed to create gobo texture: %s", SDL_GetError());
        SDL_DestroySurface(converted);
        return NULL;
    }

    Uint32 row_bytes   = w * BYTES_PER_PIXEL;
    Uint32 total_bytes = w * h * BYTES_PER_PIXEL;

    SDL_GPUTransferBufferCreateInfo xfer_info;
    SDL_zero(xfer_info);
    xfer_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    xfer_info.size  = total_bytes;

    SDL_GPUTransferBuffer *xfer = SDL_CreateGPUTransferBuffer(device, &xfer_info);
    if (!xfer) {
        SDL_Log("Failed to create gobo transfer buffer: %s", SDL_GetError());
        SDL_ReleaseGPUTexture(device, tex);
        SDL_DestroySurface(converted);
        return NULL;
    }

    void *mapped = SDL_MapGPUTransferBuffer(device, xfer, false);
    if (!mapped) {
        SDL_Log("Failed to map gobo transfer buffer: %s", SDL_GetError());
        SDL_ReleaseGPUTransferBuffer(device, xfer);
        SDL_ReleaseGPUTexture(device, tex);
        SDL_DestroySurface(converted);
        return NULL;
    }

    {
        const Uint8 *row_src = (const Uint8 *)converted->pixels;
        Uint8 *row_dst = (Uint8 *)mapped;
        for (Uint32 row = 0; row < h; row++) {
            SDL_memcpy(row_dst + row * row_bytes,
                       row_src + row * converted->pitch, row_bytes);
        }
    }
    SDL_UnmapGPUTransferBuffer(device, xfer);
    SDL_DestroySurface(converted);

    SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(device);
    if (!cmd) {
        SDL_Log("Failed to acquire cmd for gobo upload: %s", SDL_GetError());
        SDL_ReleaseGPUTransferBuffer(device, xfer);
        SDL_ReleaseGPUTexture(device, tex);
        return NULL;
    }

    SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(cmd);

    SDL_GPUTextureTransferInfo src;
    SDL_zero(src);
    src.transfer_buffer = xfer;

    SDL_GPUTextureRegion dst;
    SDL_zero(dst);
    dst.texture = tex;
    dst.w       = w;
    dst.h       = h;
    dst.d       = 1;

    SDL_UploadToGPUTexture(copy, &src, &dst, false);
    SDL_EndGPUCopyPass(copy);

    if (!SDL_SubmitGPUCommandBuffer(cmd)) {
        SDL_Log("Failed to submit gobo upload: %s", SDL_GetError());
        SDL_ReleaseGPUTransferBuffer(device, xfer);
        SDL_ReleaseGPUTexture(device, tex);
        return NULL;
    }

    SDL_ReleaseGPUTransferBuffer(device, xfer);
    return tex;
}

/* ── Helper: free model GPU resources ───────────────────────────────────── */

static void free_model_gpu(SDL_GPUDevice *device, ModelData *model)
{
    if (model->primitives) {
        for (int i = 0; i < model->primitive_count; i++) {
            if (model->primitives[i].vertex_buffer)
                SDL_ReleaseGPUBuffer(device, model->primitives[i].vertex_buffer);
            if (model->primitives[i].index_buffer)
                SDL_ReleaseGPUBuffer(device, model->primitives[i].index_buffer);
        }
        SDL_free(model->primitives);
        model->primitives = NULL;
    }

    if (model->materials) {
        for (int i = 0; i < model->material_count; i++) {
            if (!model->materials[i].texture) continue;
            /* Avoid double-free for shared textures. */
            bool already_released = false;
            for (int j = 0; j < i; j++) {
                if (model->materials[j].texture == model->materials[i].texture) {
                    already_released = true;
                    break;
                }
            }
            if (!already_released)
                SDL_ReleaseGPUTexture(device, model->materials[i].texture);
        }
        SDL_free(model->materials);
        model->materials = NULL;
    }

    forge_gltf_free(&model->scene);
}

/* ── Helper: upload glTF model to GPU ───────────────────────────────────── */

static bool upload_model_to_gpu(SDL_GPUDevice *device, ModelData *model)
{
    ForgeGltfScene *scene = &model->scene;

    model->primitive_count = scene->primitive_count;
    model->primitives = (GpuPrimitive *)SDL_calloc(
        (size_t)scene->primitive_count, sizeof(GpuPrimitive));
    if (!model->primitives) {
        SDL_Log("Failed to allocate GPU primitives");
        return false;
    }

    for (int i = 0; i < scene->primitive_count; i++) {
        const ForgeGltfPrimitive *src = &scene->primitives[i];
        GpuPrimitive *dst = &model->primitives[i];

        dst->material_index = src->material_index;
        dst->index_count    = src->index_count;
        dst->has_uvs        = src->has_uvs;

        if (src->vertices && src->vertex_count > 0) {
            Uint32 vb_size = src->vertex_count * (Uint32)sizeof(ForgeGltfVertex);
            dst->vertex_buffer = upload_gpu_buffer(
                device, SDL_GPU_BUFFERUSAGE_VERTEX, src->vertices, vb_size);
            if (!dst->vertex_buffer) {
                free_model_gpu(device, model);
                return false;
            }
        }

        if (src->indices && src->index_count > 0) {
            Uint32 ib_size = src->index_count * src->index_stride;
            dst->index_buffer = upload_gpu_buffer(
                device, SDL_GPU_BUFFERUSAGE_INDEX, src->indices, ib_size);
            if (!dst->index_buffer) {
                free_model_gpu(device, model);
                return false;
            }
            dst->index_type = (src->index_stride == 2)
                ? SDL_GPU_INDEXELEMENTSIZE_16BIT
                : SDL_GPU_INDEXELEMENTSIZE_32BIT;
        }
    }

    /* Load materials and textures. */
    model->material_count = scene->material_count;
    model->materials = (GpuMaterial *)SDL_calloc(
        (size_t)(scene->material_count > 0 ? scene->material_count : 1),
        sizeof(GpuMaterial));
    if (!model->materials) {
        SDL_Log("Failed to allocate GPU materials");
        free_model_gpu(device, model);
        return false;
    }

    {
        SDL_GPUTexture *loaded_textures[FORGE_GLTF_MAX_IMAGES];
        const char     *loaded_paths[FORGE_GLTF_MAX_IMAGES];
        int             loaded_count = 0;
        SDL_memset(loaded_textures, 0, sizeof(loaded_textures));
        SDL_memset((void *)loaded_paths, 0, sizeof(loaded_paths));

        for (int i = 0; i < scene->material_count; i++) {
            const ForgeGltfMaterial *src = &scene->materials[i];
            GpuMaterial *dst = &model->materials[i];

            dst->base_color[0] = src->base_color[0];
            dst->base_color[1] = src->base_color[1];
            dst->base_color[2] = src->base_color[2];
            dst->base_color[3] = src->base_color[3];
            dst->has_texture   = src->has_texture;
            dst->texture       = NULL;

            if (src->has_texture && src->texture_path[0] != '\0') {
                /* De-duplicate: reuse already-loaded textures. */
                bool found = false;
                for (int j = 0; j < loaded_count; j++) {
                    if (loaded_paths[j] &&
                        SDL_strcmp(loaded_paths[j], src->texture_path) == 0) {
                        dst->texture = loaded_textures[j];
                        found = true;
                        break;
                    }
                }

                if (!found && loaded_count < FORGE_GLTF_MAX_IMAGES) {
                    dst->texture = load_texture(device, src->texture_path);
                    if (dst->texture) {
                        loaded_textures[loaded_count] = dst->texture;
                        loaded_paths[loaded_count]    = src->texture_path;
                        loaded_count++;
                    } else {
                        dst->has_texture = false;
                    }
                }
            }
        }
    }

    return true;
}

/* ── Helper: load + upload a glTF model ─────────────────────────────────── */

static bool setup_model(SDL_GPUDevice *device, ModelData *model, const char *path)
{
    if (!forge_gltf_load(path, &model->scene)) {
        SDL_Log("Failed to load glTF: %s", path);
        return false;
    }
    return upload_model_to_gpu(device, model);
}

/* ── Helper: (re)create depth buffer ────────────────────────────────────── */

static bool ensure_depth_texture(app_state *state, Uint32 w, Uint32 h)
{
    if (state->depth_texture && state->depth_width == w &&
        state->depth_height == h) {
        return true;
    }

    if (state->depth_texture) {
        SDL_ReleaseGPUTexture(state->device, state->depth_texture);
        state->depth_texture = NULL;
    }

    SDL_GPUTextureCreateInfo ti;
    SDL_zero(ti);
    ti.type                = SDL_GPU_TEXTURETYPE_2D;
    ti.format              = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
    ti.width               = w;
    ti.height              = h;
    ti.layer_count_or_depth = 1;
    ti.num_levels          = 1;
    ti.usage               = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;

    state->depth_texture = SDL_CreateGPUTexture(state->device, &ti);
    if (!state->depth_texture) {
        SDL_Log("Failed to create depth texture: %s", SDL_GetError());
        return false;
    }

    state->depth_width  = w;
    state->depth_height = h;
    return true;
}

/* ── Helper: generate box placements ────────────────────────────────────── */

static void generate_box_placements(app_state *state)
{
    /* Scatter crates in the spotlight's target area. */
    const vec3 positions[BOX_COUNT] = {
        {  2.0f, 0.5f,  1.0f },
        { -2.5f, 0.5f,  0.5f },
        {  3.0f, 0.5f, -2.0f },
        { -1.0f, 0.5f, -3.0f },
        {  0.5f, 1.5f,  1.0f },  /* stacked on first crate */
    };
    const float rotations[BOX_COUNT] = {
        0.3f, 1.1f, 0.7f, 2.0f, 0.9f
    };

    for (int i = 0; i < BOX_COUNT; i++) {
        state->box_placements[i].position   = positions[i];
        state->box_placements[i].y_rotation = rotations[i];
    }
}

/* ── Helper: draw a model with the scene pipeline ───────────────────────── */

static void draw_model_scene(
    SDL_GPURenderPass *pass,
    SDL_GPUCommandBuffer *cmd,
    const ModelData *model,
    const app_state *state,
    const mat4 *placement,
    const mat4 *cam_vp)
{
    const ForgeGltfScene *scene = &model->scene;

    for (int ni = 0; ni < scene->node_count; ni++) {
        const ForgeGltfNode *node = &scene->nodes[ni];
        if (node->mesh_index < 0 || node->mesh_index >= scene->mesh_count)
            continue;

        mat4 model_mat = mat4_multiply(*placement, node->world_transform);
        mat4 mvp       = mat4_multiply(*cam_vp, model_mat);

        SceneVertUniforms vert_u;
        vert_u.mvp   = mvp;
        vert_u.model = model_mat;
        SDL_PushGPUVertexUniformData(cmd, 0, &vert_u, sizeof(vert_u));

        const ForgeGltfMesh *mesh = &scene->meshes[node->mesh_index];
        for (int pi = 0; pi < mesh->primitive_count; pi++) {
            int prim_idx = mesh->first_primitive + pi;
            const GpuPrimitive *gpu_prim = &model->primitives[prim_idx];

            if (!gpu_prim->vertex_buffer || !gpu_prim->index_buffer)
                continue;

            SDL_GPUTexture *tex = state->white_texture;

            SceneFragUniforms frag_u;
            SDL_zero(frag_u);

            if (gpu_prim->material_index >= 0 &&
                gpu_prim->material_index < model->material_count) {
                const GpuMaterial *mat = &model->materials[gpu_prim->material_index];
                frag_u.base_color[0] = mat->base_color[0];
                frag_u.base_color[1] = mat->base_color[1];
                frag_u.base_color[2] = mat->base_color[2];
                frag_u.base_color[3] = mat->base_color[3];
                frag_u.has_texture = mat->has_texture ? 1.0f : 0.0f;
                if (mat->texture)
                    tex = mat->texture;
            } else {
                frag_u.base_color[0] = 1.0f;
                frag_u.base_color[1] = 1.0f;
                frag_u.base_color[2] = 1.0f;
                frag_u.base_color[3] = 1.0f;
                frag_u.has_texture   = 0.0f;
            }

            frag_u.eye_pos[0]    = state->cam_position.x;
            frag_u.eye_pos[1]    = state->cam_position.y;
            frag_u.eye_pos[2]    = state->cam_position.z;
            frag_u.ambient       = MATERIAL_AMBIENT;
            frag_u.fill_intensity = FILL_INTENSITY;
            frag_u.shininess     = MATERIAL_SHININESS;
            frag_u.specular_str  = MATERIAL_SPECULAR_STR;
            frag_u.fill_dir[0]   = FILL_DIR_X;
            frag_u.fill_dir[1]   = FILL_DIR_Y;
            frag_u.fill_dir[2]   = FILL_DIR_Z;
            frag_u.fill_dir[3]   = 0.0f;

            /* Spotlight parameters. */
            frag_u.spot_pos[0]   = SPOT_POS_X;
            frag_u.spot_pos[1]   = SPOT_POS_Y;
            frag_u.spot_pos[2]   = SPOT_POS_Z;
            frag_u.spot_intensity = SPOT_INTENSITY;
            frag_u.spot_dir[0]   = state->spot_dir.x;
            frag_u.spot_dir[1]   = state->spot_dir.y;
            frag_u.spot_dir[2]   = state->spot_dir.z;
            frag_u.cos_inner     = SDL_cosf(SPOT_INNER_DEG * FORGE_DEG2RAD);
            frag_u.spot_color[0] = SPOT_COLOR_R;
            frag_u.spot_color[1] = SPOT_COLOR_G;
            frag_u.spot_color[2] = SPOT_COLOR_B;
            frag_u.cos_outer     = SDL_cosf(SPOT_OUTER_DEG * FORGE_DEG2RAD);
            frag_u.light_vp      = state->light_vp;

            SDL_PushGPUFragmentUniformData(cmd, 0, &frag_u, sizeof(frag_u));

            /* Bind 3 samplers: diffuse, shadow depth, gobo pattern. */
            SDL_GPUTextureSamplerBinding tex_binds[3];
            tex_binds[0] = (SDL_GPUTextureSamplerBinding){
                .texture = tex, .sampler = state->sampler };
            tex_binds[1] = (SDL_GPUTextureSamplerBinding){
                .texture = state->shadow_depth_texture,
                .sampler = state->shadow_sampler };
            tex_binds[2] = (SDL_GPUTextureSamplerBinding){
                .texture = state->gobo_texture,
                .sampler = state->gobo_sampler };
            SDL_BindGPUFragmentSamplers(pass, 0, tex_binds, 3);

            SDL_GPUBufferBinding vb;
            SDL_zero(vb);
            vb.buffer = gpu_prim->vertex_buffer;
            SDL_BindGPUVertexBuffers(pass, 0, &vb, 1);

            SDL_GPUBufferBinding ib;
            SDL_zero(ib);
            ib.buffer = gpu_prim->index_buffer;
            SDL_BindGPUIndexBuffer(pass, &ib, gpu_prim->index_type);

            SDL_DrawGPUIndexedPrimitives(pass, gpu_prim->index_count, 1, 0, 0, 0);
        }
    }
}

/* ── Helper: draw a model into the shadow map (depth-only) ─────────────── */

static void draw_model_shadow(
    SDL_GPURenderPass *pass,
    SDL_GPUCommandBuffer *cmd,
    const ModelData *model,
    const mat4 *placement,
    const mat4 *light_vp)
{
    const ForgeGltfScene *scene = &model->scene;

    for (int ni = 0; ni < scene->node_count; ni++) {
        const ForgeGltfNode *node = &scene->nodes[ni];
        if (node->mesh_index < 0 || node->mesh_index >= scene->mesh_count)
            continue;

        mat4 model_mat = mat4_multiply(*placement, node->world_transform);
        ShadowVertUniforms vert_u;
        vert_u.light_mvp = mat4_multiply(*light_vp, model_mat);
        SDL_PushGPUVertexUniformData(cmd, 0, &vert_u, sizeof(vert_u));

        const ForgeGltfMesh *mesh = &scene->meshes[node->mesh_index];
        for (int pi = 0; pi < mesh->primitive_count; pi++) {
            int prim_idx = mesh->first_primitive + pi;
            const GpuPrimitive *gpu_prim = &model->primitives[prim_idx];

            if (!gpu_prim->vertex_buffer || !gpu_prim->index_buffer)
                continue;

            SDL_GPUBufferBinding vb;
            SDL_zero(vb);
            vb.buffer = gpu_prim->vertex_buffer;
            SDL_BindGPUVertexBuffers(pass, 0, &vb, 1);

            SDL_GPUBufferBinding ib;
            SDL_zero(ib);
            ib.buffer = gpu_prim->index_buffer;
            SDL_BindGPUIndexBuffer(pass, &ib, gpu_prim->index_type);

            SDL_DrawGPUIndexedPrimitives(pass, gpu_prim->index_count, 1, 0, 0, 0);
        }
    }
}

/* ── SDL_AppInit ────────────────────────────────────────────────────────── */

SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    SDL_GPUDevice *device = SDL_CreateGPUDevice(
        SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_DXIL, true, NULL);
    if (!device) {
        SDL_Log("SDL_CreateGPUDevice failed: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    SDL_Window *window = SDL_CreateWindow(
        "Lesson 24 \xe2\x80\x94 Gobo Spotlight", WINDOW_WIDTH, WINDOW_HEIGHT, 0);
    if (!window) {
        SDL_Log("SDL_CreateWindow failed: %s", SDL_GetError());
        SDL_DestroyGPUDevice(device);
        return SDL_APP_FAILURE;
    }

    if (!SDL_ClaimWindowForGPUDevice(device, window)) {
        SDL_Log("SDL_ClaimWindowForGPUDevice failed: %s", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_DestroyGPUDevice(device);
        return SDL_APP_FAILURE;
    }

    /* Request SDR_LINEAR for correct gamma handling (sRGB swapchain). */
    if (SDL_WindowSupportsGPUSwapchainComposition(
            device, window, SDL_GPU_SWAPCHAINCOMPOSITION_SDR_LINEAR)) {
        if (!SDL_SetGPUSwapchainParameters(
                device, window, SDL_GPU_SWAPCHAINCOMPOSITION_SDR_LINEAR,
                SDL_GPU_PRESENTMODE_VSYNC)) {
            SDL_Log("SDL_SetGPUSwapchainParameters failed: %s", SDL_GetError());
            SDL_DestroyWindow(window);
            SDL_DestroyGPUDevice(device);
            return SDL_APP_FAILURE;
        }
    }

    SDL_GPUTextureFormat swapchain_format =
        SDL_GetGPUSwapchainTextureFormat(device, window);

    /* Allocate app state. */
    app_state *state = SDL_calloc(1, sizeof(app_state));
    if (!state) {
        SDL_Log("Failed to allocate app_state");
        SDL_DestroyWindow(window);
        SDL_DestroyGPUDevice(device);
        return SDL_APP_FAILURE;
    }
    state->window           = window;
    state->device           = device;
    state->swapchain_format = swapchain_format;

    /* ── White placeholder texture ──────────────────────────────────── */
    state->white_texture = create_white_texture(device);
    if (!state->white_texture) goto init_fail;

    /* ── Sampler (trilinear + anisotropy) ───────────────────────────── */
    {
        SDL_GPUSamplerCreateInfo si;
        SDL_zero(si);
        si.min_filter        = SDL_GPU_FILTER_LINEAR;
        si.mag_filter        = SDL_GPU_FILTER_LINEAR;
        si.mipmap_mode       = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR;
        si.address_mode_u    = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
        si.address_mode_v    = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
        si.address_mode_w    = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
        si.max_anisotropy    = MAX_ANISOTROPY;
        si.enable_anisotropy = true;

        state->sampler = SDL_CreateGPUSampler(device, &si);
        if (!state->sampler) {
            SDL_Log("Failed to create sampler: %s", SDL_GetError());
            goto init_fail;
        }
    }

    /* ── Load models ────────────────────────────────────────────────── */
    {
        const char *base = SDL_GetBasePath();
        char path[512];

        SDL_snprintf(path, sizeof(path), "%s%s", base, TRUCK_MODEL_PATH);
        if (!setup_model(device, &state->truck, path))
            goto init_fail;

        SDL_snprintf(path, sizeof(path), "%s%s", base, BOX_MODEL_PATH);
        if (!setup_model(device, &state->box, path))
            goto init_fail;

        SDL_snprintf(path, sizeof(path), "%s%s", base, SEARCHLIGHT_MODEL_PATH);
        if (!setup_model(device, &state->searchlight, path))
            goto init_fail;

    }

    /* ── Scene pipeline (lit geometry → swapchain) ──────────────────── */
    {
        SDL_GPUShader *vert = create_shader(device, SDL_GPU_SHADERSTAGE_VERTEX,
            scene_vert_spirv, sizeof(scene_vert_spirv),
            scene_vert_dxil, sizeof(scene_vert_dxil), 0, 1);
        /* 3 samplers: diffuse (slot 0), shadow (slot 1), gobo (slot 2). */
        SDL_GPUShader *frag = create_shader(device, SDL_GPU_SHADERSTAGE_FRAGMENT,
            scene_frag_spirv, sizeof(scene_frag_spirv),
            scene_frag_dxil, sizeof(scene_frag_dxil), 3, 1);
        if (!vert || !frag) {
            if (vert) SDL_ReleaseGPUShader(device, vert);
            if (frag) SDL_ReleaseGPUShader(device, frag);
            goto init_fail;
        }

        SDL_GPUVertexBufferDescription vb_desc;
        SDL_zero(vb_desc);
        vb_desc.slot       = 0;
        vb_desc.pitch      = sizeof(ForgeGltfVertex);
        vb_desc.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;

        SDL_GPUVertexAttribute attrs[3];
        SDL_zero(attrs);
        attrs[0].location = 0;
        attrs[0].format   = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
        attrs[0].offset   = offsetof(ForgeGltfVertex, position);
        attrs[1].location = 1;
        attrs[1].format   = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
        attrs[1].offset   = offsetof(ForgeGltfVertex, normal);
        attrs[2].location = 2;
        attrs[2].format   = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
        attrs[2].offset   = offsetof(ForgeGltfVertex, uv);

        SDL_GPUColorTargetDescription color_desc;
        SDL_zero(color_desc);
        color_desc.format = swapchain_format;

        SDL_GPUGraphicsPipelineCreateInfo pi;
        SDL_zero(pi);
        pi.vertex_shader   = vert;
        pi.fragment_shader = frag;
        pi.vertex_input_state.vertex_buffer_descriptions = &vb_desc;
        pi.vertex_input_state.num_vertex_buffers         = 1;
        pi.vertex_input_state.vertex_attributes          = attrs;
        pi.vertex_input_state.num_vertex_attributes      = 3;
        pi.primitive_type                  = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
        pi.rasterizer_state.cull_mode      = SDL_GPU_CULLMODE_BACK;
        pi.rasterizer_state.front_face     = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
        pi.rasterizer_state.fill_mode      = SDL_GPU_FILLMODE_FILL;
        pi.depth_stencil_state.compare_op       = SDL_GPU_COMPAREOP_LESS;
        pi.depth_stencil_state.enable_depth_test  = true;
        pi.depth_stencil_state.enable_depth_write = true;
        pi.target_info.color_target_descriptions  = &color_desc;
        pi.target_info.num_color_targets          = 1;
        pi.target_info.depth_stencil_format       = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
        pi.target_info.has_depth_stencil_target   = true;

        state->scene_pipeline = SDL_CreateGPUGraphicsPipeline(device, &pi);
        SDL_ReleaseGPUShader(device, vert);
        SDL_ReleaseGPUShader(device, frag);
        if (!state->scene_pipeline) {
            SDL_Log("Failed to create scene pipeline: %s", SDL_GetError());
            goto init_fail;
        }
    }

    /* ── Grid pipeline ──────────────────────────────────────────────── */
    {
        SDL_GPUShader *vert = create_shader(device, SDL_GPU_SHADERSTAGE_VERTEX,
            grid_vert_spirv, sizeof(grid_vert_spirv),
            grid_vert_dxil, sizeof(grid_vert_dxil), 0, 1);
        /* 2 samplers: shadow (slot 0), gobo (slot 1). */
        SDL_GPUShader *frag = create_shader(device, SDL_GPU_SHADERSTAGE_FRAGMENT,
            grid_frag_spirv, sizeof(grid_frag_spirv),
            grid_frag_dxil, sizeof(grid_frag_dxil), 2, 1);
        if (!vert || !frag) {
            if (vert) SDL_ReleaseGPUShader(device, vert);
            if (frag) SDL_ReleaseGPUShader(device, frag);
            goto init_fail;
        }

        SDL_GPUVertexBufferDescription vb_desc;
        SDL_zero(vb_desc);
        vb_desc.slot       = 0;
        vb_desc.pitch      = sizeof(float) * 3;
        vb_desc.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;

        SDL_GPUVertexAttribute attr;
        SDL_zero(attr);
        attr.location = 0;
        attr.format   = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
        attr.offset   = 0;

        SDL_GPUColorTargetDescription color_desc;
        SDL_zero(color_desc);
        color_desc.format = swapchain_format;

        SDL_GPUGraphicsPipelineCreateInfo pi;
        SDL_zero(pi);
        pi.vertex_shader   = vert;
        pi.fragment_shader = frag;
        pi.vertex_input_state.vertex_buffer_descriptions = &vb_desc;
        pi.vertex_input_state.num_vertex_buffers         = 1;
        pi.vertex_input_state.vertex_attributes          = &attr;
        pi.vertex_input_state.num_vertex_attributes      = 1;
        pi.primitive_type                  = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
        pi.rasterizer_state.cull_mode      = SDL_GPU_CULLMODE_NONE;
        pi.rasterizer_state.fill_mode      = SDL_GPU_FILLMODE_FILL;
        pi.depth_stencil_state.compare_op       = SDL_GPU_COMPAREOP_LESS;
        pi.depth_stencil_state.enable_depth_test  = true;
        pi.depth_stencil_state.enable_depth_write = true;
        pi.target_info.color_target_descriptions  = &color_desc;
        pi.target_info.num_color_targets          = 1;
        pi.target_info.depth_stencil_format       = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
        pi.target_info.has_depth_stencil_target   = true;

        state->grid_pipeline = SDL_CreateGPUGraphicsPipeline(device, &pi);
        SDL_ReleaseGPUShader(device, vert);
        SDL_ReleaseGPUShader(device, frag);
        if (!state->grid_pipeline) {
            SDL_Log("Failed to create grid pipeline: %s", SDL_GetError());
            goto init_fail;
        }
    }

    /* ── Shadow pipeline (depth-only pass from spotlight's perspective) */
    {
        SDL_GPUShader *vert = create_shader(device, SDL_GPU_SHADERSTAGE_VERTEX,
            shadow_vert_spirv, sizeof(shadow_vert_spirv),
            shadow_vert_dxil, sizeof(shadow_vert_dxil), 0, 1);
        /* Shadow fragment shader — no samplers, no uniforms (hardware depth write). */
        SDL_GPUShader *frag = create_shader(device, SDL_GPU_SHADERSTAGE_FRAGMENT,
            shadow_frag_spirv, sizeof(shadow_frag_spirv),
            shadow_frag_dxil, sizeof(shadow_frag_dxil), 0, 0);
        if (!vert || !frag) {
            if (vert) SDL_ReleaseGPUShader(device, vert);
            if (frag) SDL_ReleaseGPUShader(device, frag);
            goto init_fail;
        }

        /* Same vertex layout as scene pipeline (glTF vertices: pos+normal+uv).
         * The shadow vertex shader only reads position, but the buffer pitch
         * must match the actual vertex stride. */
        SDL_GPUVertexBufferDescription vb_desc;
        SDL_zero(vb_desc);
        vb_desc.slot       = 0;
        vb_desc.pitch      = sizeof(ForgeGltfVertex);
        vb_desc.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;

        SDL_GPUVertexAttribute attrs[3];
        SDL_zero(attrs);
        attrs[0].location = 0;
        attrs[0].format   = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
        attrs[0].offset   = offsetof(ForgeGltfVertex, position);
        attrs[1].location = 1;
        attrs[1].format   = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
        attrs[1].offset   = offsetof(ForgeGltfVertex, normal);
        attrs[2].location = 2;
        attrs[2].format   = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
        attrs[2].offset   = offsetof(ForgeGltfVertex, uv);

        SDL_GPUGraphicsPipelineCreateInfo pi;
        SDL_zero(pi);
        pi.vertex_shader   = vert;
        pi.fragment_shader = frag;
        pi.vertex_input_state.vertex_buffer_descriptions = &vb_desc;
        pi.vertex_input_state.num_vertex_buffers         = 1;
        pi.vertex_input_state.vertex_attributes          = attrs;
        pi.vertex_input_state.num_vertex_attributes      = 3;
        pi.primitive_type                  = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
        pi.rasterizer_state.cull_mode      = SDL_GPU_CULLMODE_BACK;
        pi.rasterizer_state.front_face     = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
        pi.rasterizer_state.fill_mode      = SDL_GPU_FILLMODE_FILL;
        pi.depth_stencil_state.compare_op       = SDL_GPU_COMPAREOP_LESS;
        pi.depth_stencil_state.enable_depth_test  = true;
        pi.depth_stencil_state.enable_depth_write = true;
        /* No color targets — depth-only pass. */
        pi.target_info.num_color_targets        = 0;
        pi.target_info.depth_stencil_format     = SHADOW_DEPTH_FMT;
        pi.target_info.has_depth_stencil_target = true;

        state->shadow_pipeline = SDL_CreateGPUGraphicsPipeline(device, &pi);
        SDL_ReleaseGPUShader(device, vert);
        SDL_ReleaseGPUShader(device, frag);
        if (!state->shadow_pipeline) {
            SDL_Log("Failed to create shadow pipeline: %s", SDL_GetError());
            goto init_fail;
        }
    }

    /* ── Grid geometry (flat quad on XZ plane) ──────────────────────── */
    {
        float verts[] = {
            -GRID_HALF_SIZE, 0.0f, -GRID_HALF_SIZE,
             GRID_HALF_SIZE, 0.0f, -GRID_HALF_SIZE,
             GRID_HALF_SIZE, 0.0f,  GRID_HALF_SIZE,
            -GRID_HALF_SIZE, 0.0f,  GRID_HALF_SIZE,
        };
        Uint16 indices[] = { 0, 1, 2, 0, 2, 3 };

        state->grid_vertex_buffer = upload_gpu_buffer(device,
            SDL_GPU_BUFFERUSAGE_VERTEX, verts, sizeof(verts));
        state->grid_index_buffer = upload_gpu_buffer(device,
            SDL_GPU_BUFFERUSAGE_INDEX, indices, sizeof(indices));

        if (!state->grid_vertex_buffer || !state->grid_index_buffer)
            goto init_fail;
    }

    /* ── Shadow depth texture (1024x1024 from spotlight's frustum) ──── */
    {
        SDL_GPUTextureCreateInfo ti;
        SDL_zero(ti);
        ti.type                = SDL_GPU_TEXTURETYPE_2D;
        ti.format              = SHADOW_DEPTH_FMT;
        ti.width               = SHADOW_MAP_SIZE;
        ti.height              = SHADOW_MAP_SIZE;
        ti.layer_count_or_depth = 1;
        ti.num_levels          = 1;
        ti.usage               = SDL_GPU_TEXTUREUSAGE_SAMPLER |
                                 SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;

        state->shadow_depth_texture = SDL_CreateGPUTexture(device, &ti);
        if (!state->shadow_depth_texture) {
            SDL_Log("Failed to create shadow depth texture: %s", SDL_GetError());
            goto init_fail;
        }
    }

    /* ── Shadow sampler (nearest, clamp — we do manual PCF in shader) ── */
    {
        SDL_GPUSamplerCreateInfo si;
        SDL_zero(si);
        si.min_filter     = SDL_GPU_FILTER_NEAREST;
        si.mag_filter     = SDL_GPU_FILTER_NEAREST;
        si.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        si.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        si.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;

        state->shadow_sampler = SDL_CreateGPUSampler(device, &si);
        if (!state->shadow_sampler) {
            SDL_Log("Failed to create shadow sampler: %s", SDL_GetError());
            goto init_fail;
        }
    }

    /* ── Gobo sampler (linear, clamp — smooth projected pattern) ───── */
    {
        SDL_GPUSamplerCreateInfo si;
        SDL_zero(si);
        si.min_filter     = SDL_GPU_FILTER_LINEAR;
        si.mag_filter     = SDL_GPU_FILTER_LINEAR;
        si.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        si.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        si.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;

        state->gobo_sampler = SDL_CreateGPUSampler(device, &si);
        if (!state->gobo_sampler) {
            SDL_Log("Failed to create gobo sampler: %s", SDL_GetError());
            goto init_fail;
        }
    }

    /* ── Load gobo pattern texture ─────────────────────────────────── */
    {
        const char *base = SDL_GetBasePath();
        char gobo_path[512];
        SDL_snprintf(gobo_path, sizeof(gobo_path), "%s%s", base, GOBO_TEXTURE_PATH);

        state->gobo_texture = load_gobo_texture(device, gobo_path);
        if (!state->gobo_texture) goto init_fail;
    }

    /* ── Scene placement ────────────────────────────────────────────── */
    generate_box_placements(state);

    /* Searchlight: scale down, raise to sit on the ground, rotate to
     * face the truck (225 degrees clockwise from +Z). */
    {
        mat4 scale     = mat4_scale(vec3_create(
            SEARCHLIGHT_SCALE, SEARCHLIGHT_SCALE, SEARCHLIGHT_SCALE));
        mat4 rotate    = mat4_rotate_y(225.0f * FORGE_DEG2RAD); /* 225 deg CW */
        mat4 translate = mat4_translate(vec3_create(6.0f, 1.0f, 4.0f));
        /* T * R * S — scale first, then rotate, then translate. */
        state->searchlight_placement = mat4_multiply(
            translate, mat4_multiply(rotate, scale));
    }

    /* ── Spotlight view-projection (static — light doesn't move) ──── */
    {
        vec3 spot_pos    = vec3_create(SPOT_POS_X, SPOT_POS_Y, SPOT_POS_Z);
        vec3 spot_target = vec3_create(SPOT_TARGET_X, SPOT_TARGET_Y, SPOT_TARGET_Z);
        vec3 spot_up     = vec3_create(0.0f, 1.0f, 0.0f);

        mat4 light_view = mat4_look_at(spot_pos, spot_target, spot_up);
        /* FOV = 2 * outer cone half-angle to fully cover the spotlight cone. */
        float outer_rad  = SPOT_OUTER_DEG * FORGE_DEG2RAD;
        mat4 light_proj  = mat4_perspective(2.0f * outer_rad, 1.0f,
                                            SPOT_NEAR, SPOT_FAR);
        state->light_vp = mat4_multiply(light_proj, light_view);
        state->spot_dir = vec3_normalize(vec3_sub(spot_target, spot_pos));
    }

    /* ── Camera initial state ───────────────────────────────────────── */
    state->cam_position = vec3_create(CAM_START_X, CAM_START_Y, CAM_START_Z);
    state->cam_yaw      = CAM_START_YAW_DEG * FORGE_DEG2RAD;
    state->cam_pitch    = CAM_START_PITCH_DEG * FORGE_DEG2RAD;

    /* Capture mouse for FPS camera. */
    if (!SDL_SetWindowRelativeMouseMode(window, true)) {
        SDL_Log("SDL_SetWindowRelativeMouseMode failed: %s", SDL_GetError());
    }
    state->mouse_captured = true;

    state->last_ticks = SDL_GetPerformanceCounter();

    *appstate = state;
    return SDL_APP_CONTINUE;

init_fail:
    /* Centralized cleanup for init failures. */
    if (state->scene_pipeline)
        SDL_ReleaseGPUGraphicsPipeline(device, state->scene_pipeline);
    if (state->grid_pipeline)
        SDL_ReleaseGPUGraphicsPipeline(device, state->grid_pipeline);
    if (state->shadow_pipeline)
        SDL_ReleaseGPUGraphicsPipeline(device, state->shadow_pipeline);
    if (state->grid_vertex_buffer)
        SDL_ReleaseGPUBuffer(device, state->grid_vertex_buffer);
    if (state->grid_index_buffer)
        SDL_ReleaseGPUBuffer(device, state->grid_index_buffer);
    if (state->sampler)
        SDL_ReleaseGPUSampler(device, state->sampler);
    if (state->shadow_sampler)
        SDL_ReleaseGPUSampler(device, state->shadow_sampler);
    if (state->gobo_sampler)
        SDL_ReleaseGPUSampler(device, state->gobo_sampler);
    if (state->white_texture)
        SDL_ReleaseGPUTexture(device, state->white_texture);
    if (state->shadow_depth_texture)
        SDL_ReleaseGPUTexture(device, state->shadow_depth_texture);
    if (state->gobo_texture)
        SDL_ReleaseGPUTexture(device, state->gobo_texture);
    if (state->depth_texture)
        SDL_ReleaseGPUTexture(device, state->depth_texture);
    free_model_gpu(device, &state->truck);
    free_model_gpu(device, &state->box);
    free_model_gpu(device, &state->searchlight);
    SDL_free(state);
    SDL_ReleaseWindowFromGPUDevice(device, window);
    SDL_DestroyWindow(window);
    SDL_DestroyGPUDevice(device);
    return SDL_APP_FAILURE;
}

/* ── SDL_AppEvent ───────────────────────────────────────────────────────── */

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{
    app_state *state = (app_state *)appstate;

    if (event->type == SDL_EVENT_QUIT) {
        return SDL_APP_SUCCESS;
    }

    if (event->type == SDL_EVENT_KEY_DOWN) {
        SDL_Keycode key = event->key.key;

        if (key == SDLK_ESCAPE) {
            if (state->mouse_captured) {
                SDL_SetWindowRelativeMouseMode(state->window, false);
                state->mouse_captured = false;
            } else {
                return SDL_APP_SUCCESS;
            }
        }
    }

    /* Re-capture mouse on click. */
    if (event->type == SDL_EVENT_MOUSE_BUTTON_DOWN && !state->mouse_captured) {
        SDL_SetWindowRelativeMouseMode(state->window, true);
        state->mouse_captured = true;
    }

    /* Mouse look. */
    if (event->type == SDL_EVENT_MOUSE_MOTION && state->mouse_captured) {
        state->cam_yaw   -= event->motion.xrel * MOUSE_SENS;
        state->cam_pitch -= event->motion.yrel * MOUSE_SENS;
        if (state->cam_pitch > PITCH_CLAMP) state->cam_pitch = PITCH_CLAMP;
        if (state->cam_pitch < -PITCH_CLAMP) state->cam_pitch = -PITCH_CLAMP;
    }

    return SDL_APP_CONTINUE;
}

/* ── SDL_AppIterate ─────────────────────────────────────────────────────── */

SDL_AppResult SDL_AppIterate(void *appstate)
{
    app_state *state = (app_state *)appstate;

    /* ── Delta time ─────────────────────────────────────────────────── */
    Uint64 now  = SDL_GetPerformanceCounter();
    float  freq = (float)SDL_GetPerformanceFrequency();
    float  dt   = (float)(now - state->last_ticks) / freq;
    state->last_ticks = now;
    if (dt > MAX_FRAME_DT) dt = MAX_FRAME_DT;

    /* ── Keyboard movement ──────────────────────────────────────────── */
    {
        const bool *keys = SDL_GetKeyboardState(NULL);
        if (state->mouse_captured) {
            quat orientation = quat_from_euler(
                state->cam_yaw, state->cam_pitch, 0.0f);
            vec3 forward = quat_forward(orientation);
            vec3 right   = quat_right(orientation);
            vec3 up      = vec3_create(0.0f, 1.0f, 0.0f);
            float speed  = CAM_SPEED * dt;

            if (keys[SDL_SCANCODE_W])
                state->cam_position = vec3_add(state->cam_position,
                    vec3_scale(forward, speed));
            if (keys[SDL_SCANCODE_S])
                state->cam_position = vec3_add(state->cam_position,
                    vec3_scale(forward, -speed));
            if (keys[SDL_SCANCODE_D])
                state->cam_position = vec3_add(state->cam_position,
                    vec3_scale(right, speed));
            if (keys[SDL_SCANCODE_A])
                state->cam_position = vec3_add(state->cam_position,
                    vec3_scale(right, -speed));
            if (keys[SDL_SCANCODE_SPACE])
                state->cam_position = vec3_add(state->cam_position,
                    vec3_scale(up, speed));
            if (keys[SDL_SCANCODE_LSHIFT])
                state->cam_position = vec3_add(state->cam_position,
                    vec3_scale(up, -speed));
        }
    }

    /* ── Camera matrices ────────────────────────────────────────────── */
    quat cam_orient = quat_from_euler(state->cam_yaw, state->cam_pitch, 0.0f);
    mat4 view   = mat4_view_from_quat(state->cam_position, cam_orient);
    float aspect = (float)WINDOW_WIDTH / (float)WINDOW_HEIGHT;
    mat4 proj   = mat4_perspective(
        (float)FOV_DEG * FORGE_DEG2RAD, aspect, NEAR_PLANE, FAR_PLANE);
    mat4 cam_vp = mat4_multiply(proj, view);

    /* ── Acquire swapchain ──────────────────────────────────────────── */
    SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(state->device);
    if (!cmd) {
        SDL_Log("SDL_AcquireGPUCommandBuffer failed: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    SDL_GPUTexture *swapchain_tex = NULL;
    Uint32 sw, sh;
    if (!SDL_AcquireGPUSwapchainTexture(cmd, state->window,
                                         &swapchain_tex, &sw, &sh)) {
        SDL_Log("SDL_AcquireGPUSwapchainTexture failed: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    if (!swapchain_tex) {
        if (!SDL_SubmitGPUCommandBuffer(cmd)) {
            SDL_Log("SDL_SubmitGPUCommandBuffer failed: %s", SDL_GetError());
        }
        return SDL_APP_CONTINUE;
    }

    /* ── Ensure depth buffer matches swapchain size ─────────────────── */
    if (!ensure_depth_texture(state, sw, sh)) {
        return SDL_APP_FAILURE;
    }

    /* ── Shadow pass — render scene from spotlight's perspective ──── */
    {
        SDL_GPUDepthStencilTargetInfo shadow_depth;
        SDL_zero(shadow_depth);
        shadow_depth.texture     = state->shadow_depth_texture;
        shadow_depth.load_op     = SDL_GPU_LOADOP_CLEAR;
        shadow_depth.store_op    = SDL_GPU_STOREOP_STORE; /* read later */
        shadow_depth.clear_depth = 1.0f;

        /* No color targets — depth-only pass. */
        SDL_GPURenderPass *shadow_pass = SDL_BeginGPURenderPass(
            cmd, NULL, 0, &shadow_depth);

        SDL_BindGPUGraphicsPipeline(shadow_pass, state->shadow_pipeline);

        /* Draw shadow casters (truck + crates, not the searchlight). */
        mat4 truck_placement = mat4_identity();
        draw_model_shadow(shadow_pass, cmd, &state->truck,
                          &truck_placement, &state->light_vp);

        for (int i = 0; i < BOX_COUNT; i++) {
            BoxPlacement *bp = &state->box_placements[i];
            mat4 box_placement = mat4_multiply(
                mat4_translate(bp->position),
                mat4_rotate_y(bp->y_rotation));
            draw_model_shadow(shadow_pass, cmd, &state->box,
                              &box_placement, &state->light_vp);
        }

        SDL_EndGPURenderPass(shadow_pass);
    }

    /* ── Main render pass ──────────────────────────────────────────── */
    SDL_GPUColorTargetInfo color_target;
    SDL_zero(color_target);
    color_target.texture     = swapchain_tex;
    color_target.load_op     = SDL_GPU_LOADOP_CLEAR;
    color_target.store_op    = SDL_GPU_STOREOP_STORE;
    color_target.clear_color = (SDL_FColor){ CLEAR_R, CLEAR_G, CLEAR_B, 1.0f };

    SDL_GPUDepthStencilTargetInfo depth_target;
    SDL_zero(depth_target);
    depth_target.texture     = state->depth_texture;
    depth_target.load_op     = SDL_GPU_LOADOP_CLEAR;
    depth_target.store_op    = SDL_GPU_STOREOP_DONT_CARE;
    depth_target.clear_depth = 1.0f;

    SDL_GPURenderPass *pass = SDL_BeginGPURenderPass(
        cmd, &color_target, 1, &depth_target);
    if (!pass) {
        SDL_Log("SDL_BeginGPURenderPass failed: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    /* ── Draw grid ──────────────────────────────────────────────── */
    SDL_BindGPUGraphicsPipeline(pass, state->grid_pipeline);
    {
        GridVertUniforms grid_vu;
        grid_vu.vp = cam_vp;
        SDL_PushGPUVertexUniformData(cmd, 0, &grid_vu, sizeof(grid_vu));

        GridFragUniforms grid_fu;
        grid_fu.line_color[0] = GRID_LINE_R;
        grid_fu.line_color[1] = GRID_LINE_G;
        grid_fu.line_color[2] = GRID_LINE_B;
        grid_fu.line_color[3] = 1.0f;
        grid_fu.bg_color[0]   = GRID_BG_R;
        grid_fu.bg_color[1]   = GRID_BG_G;
        grid_fu.bg_color[2]   = GRID_BG_B;
        grid_fu.bg_color[3]   = 1.0f;
        grid_fu.eye_pos[0]    = state->cam_position.x;
        grid_fu.eye_pos[1]    = state->cam_position.y;
        grid_fu.eye_pos[2]    = state->cam_position.z;
        grid_fu.grid_spacing  = GRID_SPACING;
        grid_fu.line_width    = GRID_LINE_WIDTH;
        grid_fu.fade_distance = GRID_FADE_DISTANCE;
        grid_fu._pad0         = 0.0f;
        grid_fu._pad1         = 0.0f;

        /* Spotlight parameters for grid floor illumination. */
        grid_fu.spot_pos[0]   = SPOT_POS_X;
        grid_fu.spot_pos[1]   = SPOT_POS_Y;
        grid_fu.spot_pos[2]   = SPOT_POS_Z;
        grid_fu.spot_intensity = SPOT_INTENSITY;
        grid_fu.spot_dir[0]   = state->spot_dir.x;
        grid_fu.spot_dir[1]   = state->spot_dir.y;
        grid_fu.spot_dir[2]   = state->spot_dir.z;
        grid_fu.cos_inner     = SDL_cosf(SPOT_INNER_DEG * FORGE_DEG2RAD);
        grid_fu.spot_color[0] = SPOT_COLOR_R;
        grid_fu.spot_color[1] = SPOT_COLOR_G;
        grid_fu.spot_color[2] = SPOT_COLOR_B;
        grid_fu.cos_outer     = SDL_cosf(SPOT_OUTER_DEG * FORGE_DEG2RAD);
        grid_fu.light_vp      = state->light_vp;

        SDL_PushGPUFragmentUniformData(cmd, 0, &grid_fu, sizeof(grid_fu));

        /* Bind 2 samplers: shadow depth, gobo pattern. */
        SDL_GPUTextureSamplerBinding grid_tex_binds[2];
        grid_tex_binds[0] = (SDL_GPUTextureSamplerBinding){
            .texture = state->shadow_depth_texture,
            .sampler = state->shadow_sampler };
        grid_tex_binds[1] = (SDL_GPUTextureSamplerBinding){
            .texture = state->gobo_texture,
            .sampler = state->gobo_sampler };
        SDL_BindGPUFragmentSamplers(pass, 0, grid_tex_binds, 2);

        SDL_GPUBufferBinding vb_bind = { state->grid_vertex_buffer, 0 };
        SDL_BindGPUVertexBuffers(pass, 0, &vb_bind, 1);

        SDL_GPUBufferBinding ib_bind = { state->grid_index_buffer, 0 };
        SDL_BindGPUIndexBuffer(pass, &ib_bind, SDL_GPU_INDEXELEMENTSIZE_16BIT);

        SDL_DrawGPUIndexedPrimitives(pass, GRID_INDEX_COUNT, 1, 0, 0, 0);
    }

    /* ── Draw scene models ──────────────────────────────────────── */
    SDL_BindGPUGraphicsPipeline(pass, state->scene_pipeline);

    /* Truck at origin. */
    {
        mat4 truck_placement = mat4_identity();
        draw_model_scene(pass, cmd, &state->truck, state,
                         &truck_placement, &cam_vp);
    }

    /* Scattered crates. */
    for (int i = 0; i < BOX_COUNT; i++) {
        BoxPlacement *bp = &state->box_placements[i];
        mat4 box_placement = mat4_multiply(
            mat4_translate(bp->position),
            mat4_rotate_y(bp->y_rotation));
        draw_model_scene(pass, cmd, &state->box, state,
                         &box_placement, &cam_vp);
    }

    /* Searchlight fixture. */
    draw_model_scene(pass, cmd, &state->searchlight, state,
                     &state->searchlight_placement, &cam_vp);

    SDL_EndGPURenderPass(pass);

    if (!SDL_SubmitGPUCommandBuffer(cmd)) {
        SDL_Log("SDL_SubmitGPUCommandBuffer failed: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    return SDL_APP_CONTINUE;
}

/* ── SDL_AppQuit ────────────────────────────────────────────────────────── */

void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
    (void)result;
    app_state *state = (app_state *)appstate;
    if (!state) return;

    free_model_gpu(state->device, &state->truck);
    free_model_gpu(state->device, &state->box);
    free_model_gpu(state->device, &state->searchlight);

    if (state->scene_pipeline)
        SDL_ReleaseGPUGraphicsPipeline(state->device, state->scene_pipeline);
    if (state->grid_pipeline)
        SDL_ReleaseGPUGraphicsPipeline(state->device, state->grid_pipeline);
    if (state->shadow_pipeline)
        SDL_ReleaseGPUGraphicsPipeline(state->device, state->shadow_pipeline);
    if (state->grid_vertex_buffer)
        SDL_ReleaseGPUBuffer(state->device, state->grid_vertex_buffer);
    if (state->grid_index_buffer)
        SDL_ReleaseGPUBuffer(state->device, state->grid_index_buffer);
    if (state->white_texture)
        SDL_ReleaseGPUTexture(state->device, state->white_texture);
    if (state->shadow_depth_texture)
        SDL_ReleaseGPUTexture(state->device, state->shadow_depth_texture);
    if (state->gobo_texture)
        SDL_ReleaseGPUTexture(state->device, state->gobo_texture);
    if (state->sampler)
        SDL_ReleaseGPUSampler(state->device, state->sampler);
    if (state->shadow_sampler)
        SDL_ReleaseGPUSampler(state->device, state->shadow_sampler);
    if (state->gobo_sampler)
        SDL_ReleaseGPUSampler(state->device, state->gobo_sampler);
    if (state->depth_texture)
        SDL_ReleaseGPUTexture(state->device, state->depth_texture);

    SDL_ReleaseWindowFromGPUDevice(state->device, state->window);
    SDL_DestroyWindow(state->window);
    SDL_DestroyGPUDevice(state->device);
    SDL_free(state);
}
