/*
 * UI Lesson 12 -- Font Scaling and Spacing
 *
 * Demonstrates: Global scale factor and consistent spacing system.
 *
 * The ForgeUiContext now carries a `scale` field (default 1.0) that
 * multiplies all widget dimensions, font pixel height, padding, and
 * spacing.  A companion ForgeUiSpacing struct on the context holds
 * base (unscaled) values for every spacing constant, accessed through
 * the FORGE_UI_SCALED(ctx, value) macro.
 *
 * This program:
 *   1. Loads a TrueType font three times at different pixel heights
 *      (base * 0.75, base * 1.0, base * 1.5) to build three atlases
 *   2. Creates three ForgeUiContexts with scales 0.75, 1.0, and 1.5
 *   3. Renders the same settings panel at all three scales side by side
 *      into a single framebuffer, showing proportional scaling
 *   4. Renders a second frame demonstrating spacing overrides: doubled
 *      widget_padding and item_spacing for a spacious layout, and
 *      halved values for a compact layout, both at scale 1.0
 *   5. Each frame is rendered via forge_raster_triangles_indexed and
 *      written as a BMP image
 *
 * Output:
 *   frame_01_three_scales.bmp   -- same panel at 0.75x, 1.0x, 1.5x
 *   frame_02_spacing_override.bmp -- spacious vs compact at 1.0x
 *
 * This is a console program -- no GPU or window is needed.
 *
 * SPDX-License-Identifier: Zlib
 */

#include <SDL3/SDL.h>
#include "ui/forge_ui.h"
#include "ui/forge_ui_ctx.h"
#include "raster/forge_raster.h"

/* ── Default font path ──────────────────────────────────────────────────── */
#define DEFAULT_FONT_PATH "assets/fonts/liberation_mono/LiberationMono-Regular.ttf"

/* ── Section separators for console output ───────────────────────────────── */
#define SEPARATOR "============================================================"
#define THIN_SEP  "------------------------------------------------------------"

/* ── Atlas parameters ────────────────────────────────────────────────────── */
#define BASE_PIXEL_HEIGHT  16.0f  /* base design pixel height (unscaled) */
#define ATLAS_PADDING       1     /* 1 pixel padding between glyphs */
#define ASCII_START        32     /* first printable ASCII codepoint (space) */
#define ASCII_END         126     /* last printable ASCII codepoint (tilde) */
#define ASCII_COUNT       (ASCII_END - ASCII_START + 1)  /* 95 glyphs */

/* ── Framebuffer dimensions ──────────────────────────────────────────────── */
#define FB_WIDTH   960  /* output image width in pixels */
#define FB_HEIGHT  400  /* output image height in pixels */

/* ── Background color (#1a1a2e as floats) ────────────────────────────────── */
#define BG_R  0.102f
#define BG_G  0.102f
#define BG_B  0.180f
#define BG_A  1.0f

/* ── Widget row heights (base values, multiplied by ctx->scale) ──────────── */
#define LABEL_ROW_HEIGHT     20.0f  /* label text row height */
#define CHECKBOX_ROW_HEIGHT  22.0f  /* checkbox row height */
#define SLIDER_ROW_HEIGHT    26.0f  /* slider row height */
#define BUTTON_ROW_HEIGHT    28.0f  /* button row height */

/* ── Label color (light gray-blue text, sRGB #e0e0f0) ────────────────────── */
#define LABEL_R  0.878f
#define LABEL_G  0.878f
#define LABEL_B  0.941f
#define LABEL_A  1.0f

/* ── Layout constants for the demo frames ─────────────────────────────────── */
#define PANEL_GAP        20.0f  /* horizontal gap between side-by-side panels */
#define PANEL_MARGIN_X   20.0f  /* left margin from framebuffer edge */
#define PANEL_MARGIN_Y   15.0f  /* top margin from framebuffer edge */
#define MOUSE_OFFSCREEN -100.0f /* offscreen mouse position (no hover/click) */
#define SLIDER_INIT_A     0.5f  /* initial slider value for frame 1 */
#define SLIDER_INIT_B     0.65f /* initial slider value for frame 2 */
#define SPACIOUS_MULT     2.0f  /* spacing multiplier for spacious layout */
#define COMPACT_MULT      0.5f  /* spacing multiplier for compact layout */
#define SPACIOUS_W      340.0f  /* spacious panel width */
#define SPACIOUS_H      370.0f  /* spacious panel height */
#define COMPACT_W       240.0f  /* compact panel width */
#define COMPACT_H       240.0f  /* compact panel height */

/* ── Widget application state (shared across all sub-panels) ─────────────── */
typedef struct DemoState {
    bool   checkbox_a;  /* "Enable shadows" toggle state */
    bool   checkbox_b;  /* "Show grid" toggle state */
    float  slider_val;  /* generic slider value (0.0 .. 1.0) */
} DemoState;

/* ── Helper: build atlas at a given pixel height ─────────────────────────── */
static bool build_atlas_at_height(const ForgeUiFont *font,
                                  float pixel_height,
                                  ForgeUiFontAtlas *atlas)
{
    Uint32 codepoints[ASCII_COUNT];
    for (int i = 0; i < ASCII_COUNT; i++) {
        codepoints[i] = (Uint32)(ASCII_START + i);
    }

    if (!forge_ui_atlas_build(font, pixel_height, codepoints, ASCII_COUNT,
                              ATLAS_PADDING, atlas)) {
        SDL_Log("Failed to build atlas at %.1f px", (double)pixel_height);
        return false;
    }
    return true;
}

/* ── Helper: render a settings sub-panel into a context ──────────────────── */
static void render_settings_panel(ForgeUiContext *ctx,
                                  const char *title,
                                  ForgeUiRect panel_rect,
                                  float *scroll_y,
                                  DemoState *ds)
{
    if (!forge_ui_ctx_panel_begin(ctx, 1, title, panel_rect, scroll_y))
        return;

    /* Title label */
    forge_ui_ctx_label_layout(ctx, "Settings",
                              LABEL_ROW_HEIGHT * ctx->scale,
                              LABEL_R, LABEL_G, LABEL_B, LABEL_A);

    /* Two checkboxes */
    forge_ui_ctx_checkbox_layout(ctx, 10, "Enable shadows",
                                 &ds->checkbox_a,
                                 CHECKBOX_ROW_HEIGHT * ctx->scale);
    forge_ui_ctx_checkbox_layout(ctx, 11, "Show grid",
                                 &ds->checkbox_b,
                                 CHECKBOX_ROW_HEIGHT * ctx->scale);

    /* Slider */
    forge_ui_ctx_slider_layout(ctx, 20, &ds->slider_val,
                               0.0f, 1.0f,
                               SLIDER_ROW_HEIGHT * ctx->scale);

    /* Button */
    forge_ui_ctx_button_layout(ctx, 30, "Apply",
                               BUTTON_ROW_HEIGHT * ctx->scale);

    forge_ui_ctx_panel_end(ctx);
}

/* ── Helper: rasterize UI context into the framebuffer ───────────────────── */
static void rasterize_ui(ForgeRasterBuffer *fb,
                          ForgeUiContext *ctx,
                          const ForgeRasterTexture *tex)
{
    ForgeRasterVertex *rv = (ForgeRasterVertex *)SDL_malloc(
        (size_t)ctx->vertex_count * sizeof(ForgeRasterVertex));
    if (!rv) return;

    for (int i = 0; i < ctx->vertex_count; i++) {
        rv[i].x  = ctx->vertices[i].pos_x;
        rv[i].y  = ctx->vertices[i].pos_y;
        rv[i].u  = ctx->vertices[i].uv_u;
        rv[i].v  = ctx->vertices[i].uv_v;
        rv[i].r  = ctx->vertices[i].r;
        rv[i].g  = ctx->vertices[i].g;
        rv[i].b  = ctx->vertices[i].b;
        rv[i].a  = ctx->vertices[i].a;
    }

    forge_raster_triangles_indexed(fb, rv, ctx->vertex_count,
                                   ctx->indices, ctx->index_count, tex);
    SDL_free(rv);
}

/* ── Helper: create raster texture from font atlas ───────────────────────── */
static ForgeRasterTexture make_atlas_texture(const ForgeUiFontAtlas *atlas)
{
    ForgeRasterTexture tex;
    tex.pixels = atlas->pixels;
    tex.width  = atlas->width;
    tex.height = atlas->height;
    return tex;
}

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    if (!SDL_Init(0)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    SDL_Log("%s", SEPARATOR);
    SDL_Log("UI Lesson 12 -- Font Scaling and Spacing");
    SDL_Log("%s", SEPARATOR);

    /* ── Load font ────────────────────────────────────────────────────── */
    ForgeUiFont font;
    if (!forge_ui_ttf_load(DEFAULT_FONT_PATH, &font)) {
        SDL_Log("Failed to load font: %s", DEFAULT_FONT_PATH);
        SDL_Quit();
        return 1;
    }
    SDL_Log("Font loaded: %s", DEFAULT_FONT_PATH);

    /* ── Build three atlases at different scaled pixel heights ─────────── */
    float scales[3] = { 0.75f, 1.0f, 1.5f };
    ForgeUiFontAtlas atlases[3];
    SDL_memset(atlases, 0, sizeof(atlases));

    for (int i = 0; i < 3; i++) {
        float ph = BASE_PIXEL_HEIGHT * scales[i];
        if (!build_atlas_at_height(&font, ph, &atlases[i])) {
            /* Clean up previously built atlases */
            for (int j = 0; j < i; j++) forge_ui_atlas_free(&atlases[j]);
            forge_ui_ttf_free(&font);
            SDL_Quit();
            return 1;
        }
        SDL_Log("Atlas %d: scale=%.2f  pixel_height=%.1f  atlas=%dx%d",
                i, (double)scales[i], (double)ph,
                atlases[i].width, atlases[i].height);
    }

    /* ── Create framebuffer ───────────────────────────────────────────── */
    ForgeRasterBuffer fb = forge_raster_buffer_create(FB_WIDTH, FB_HEIGHT);
    if (!fb.pixels) {
        SDL_Log("Failed to create framebuffer");
        for (int i = 0; i < 3; i++) forge_ui_atlas_free(&atlases[i]);
        forge_ui_ttf_free(&font);
        SDL_Quit();
        return 1;
    }

    /* ================================================================== */
    /* Frame 1: Same panel at three different scales (0.75, 1.0, 1.5)     */
    /* ================================================================== */
    SDL_Log("%s", THIN_SEP);
    SDL_Log("Frame 1: Three scales side by side");
    SDL_Log("%s", THIN_SEP);

    forge_raster_clear(&fb, BG_R, BG_G, BG_B, BG_A);

    /* Panel widths and positions for three side-by-side sub-panels */
    float panel_widths[3]  = { 200.0f, 260.0f, 360.0f };
    float panel_heights[3] = { 240.0f, 300.0f, 370.0f };
    float gap = PANEL_GAP;
    float x_cursor = PANEL_MARGIN_X;

    for (int i = 0; i < 3; i++) {
        /* Initialize context with this scale's atlas */
        ForgeUiContext ctx;
        if (!forge_ui_ctx_init(&ctx, &atlases[i])) {
            SDL_Log("Failed to init context %d", i);
            continue;
        }
        ctx.scale = scales[i];
        ctx.base_pixel_height = BASE_PIXEL_HEIGHT;

        /* Widget state (fresh per sub-panel) */
        DemoState ds = { true, false, SLIDER_INIT_A };
        float scroll_y = 0.0f;

        /* Panel rect */
        ForgeUiRect panel_rect = {
            x_cursor, PANEL_MARGIN_Y,
            panel_widths[i], panel_heights[i]
        };

        /* Build title with scale label */
        char title[64];
        SDL_snprintf(title, sizeof(title), "Scale %.2fx", (double)scales[i]);

        /* Render the frame */
        forge_ui_ctx_begin(&ctx, MOUSE_OFFSCREEN, MOUSE_OFFSCREEN, false);
        render_settings_panel(&ctx, title, panel_rect, &scroll_y, &ds);
        forge_ui_ctx_end(&ctx);

        SDL_Log("  Scale %.2f: %d vertices, %d indices",
                (double)scales[i], ctx.vertex_count, ctx.index_count);

        /* Rasterize into the shared framebuffer */
        ForgeRasterTexture tex = make_atlas_texture(&atlases[i]);
        rasterize_ui(&fb, &ctx, &tex);

        x_cursor += panel_widths[i] + gap;

        forge_ui_ctx_free(&ctx);
    }

    /* Write frame 1 */
    if (forge_raster_write_bmp(&fb, "frame_01_three_scales.bmp")) {
        SDL_Log("Wrote: frame_01_three_scales.bmp");
    }

    /* ================================================================== */
    /* Frame 2: Spacing overrides (spacious vs compact) at scale 1.0      */
    /* ================================================================== */
    SDL_Log("%s", THIN_SEP);
    SDL_Log("Frame 2: Spacing overrides at scale 1.0");
    SDL_Log("%s", THIN_SEP);

    forge_raster_clear(&fb, BG_R, BG_G, BG_B, BG_A);

    /* Two sub-panels: spacious (left) and compact (right) */
    const char *spacing_labels[2] = { "Spacious", "Compact" };
    float spacing_mults[2] = { SPACIOUS_MULT, COMPACT_MULT };

    x_cursor = PANEL_MARGIN_X;
    for (int s = 0; s < 2; s++) {
        ForgeUiContext ctx;
        if (!forge_ui_ctx_init(&ctx, &atlases[1])) {  /* 1.0x atlas */
            SDL_Log("Failed to init context for spacing demo %d", s);
            continue;
        }
        ctx.scale = 1.0f;
        ctx.base_pixel_height = BASE_PIXEL_HEIGHT;

        /* Override spacing values */
        float mult = spacing_mults[s];
        ctx.spacing.widget_padding *= mult;
        ctx.spacing.item_spacing   *= mult;

        DemoState ds = { true, false, SLIDER_INIT_B };
        float scroll_y = 0.0f;

        float pw = (s == 0) ? SPACIOUS_W : COMPACT_W;
        float ph = (s == 0) ? SPACIOUS_H : COMPACT_H;
        ForgeUiRect panel_rect = { x_cursor, PANEL_MARGIN_Y, pw, ph };

        char title[64];
        SDL_snprintf(title, sizeof(title), "%s (%.0fx spacing)",
                     spacing_labels[s], (double)mult);

        forge_ui_ctx_begin(&ctx, MOUSE_OFFSCREEN, MOUSE_OFFSCREEN, false);
        render_settings_panel(&ctx, title, panel_rect, &scroll_y, &ds);
        forge_ui_ctx_end(&ctx);

        SDL_Log("  %s: widget_padding=%.1f  item_spacing=%.1f  "
                "%d vertices, %d indices",
                spacing_labels[s],
                (double)ctx.spacing.widget_padding,
                (double)ctx.spacing.item_spacing,
                ctx.vertex_count, ctx.index_count);

        ForgeRasterTexture tex = make_atlas_texture(&atlases[1]);
        rasterize_ui(&fb, &ctx, &tex);

        x_cursor += pw + gap;

        forge_ui_ctx_free(&ctx);
    }

    /* Write frame 2 */
    if (forge_raster_write_bmp(&fb, "frame_02_spacing_override.bmp")) {
        SDL_Log("Wrote: frame_02_spacing_override.bmp");
    }

    /* ── Print spacing struct defaults ────────────────────────────────── */
    SDL_Log("%s", THIN_SEP);
    SDL_Log("ForgeUiSpacing defaults (base/unscaled values):");
    SDL_Log("%s", THIN_SEP);
    {
        ForgeUiContext tmp;
        if (forge_ui_ctx_init(&tmp, &atlases[1])) {
            SDL_Log("  widget_padding     = %.1f", (double)tmp.spacing.widget_padding);
            SDL_Log("  item_spacing       = %.1f", (double)tmp.spacing.item_spacing);
            SDL_Log("  panel_padding      = %.1f", (double)tmp.spacing.panel_padding);
            SDL_Log("  title_bar_height   = %.1f", (double)tmp.spacing.title_bar_height);
            SDL_Log("  checkbox_box_size  = %.1f", (double)tmp.spacing.checkbox_box_size);
            SDL_Log("  checkbox_inner_pad = %.1f", (double)tmp.spacing.checkbox_inner_pad);
            SDL_Log("  checkbox_label_gap = %.1f", (double)tmp.spacing.checkbox_label_gap);
            SDL_Log("  slider_thumb_width = %.1f", (double)tmp.spacing.slider_thumb_width);
            SDL_Log("  slider_thumb_height= %.1f", (double)tmp.spacing.slider_thumb_height);
            SDL_Log("  slider_track_height= %.1f", (double)tmp.spacing.slider_track_height);
            SDL_Log("  text_input_padding = %.1f", (double)tmp.spacing.text_input_padding);
            SDL_Log("  scrollbar_width    = %.1f", (double)tmp.spacing.scrollbar_width);
            forge_ui_ctx_free(&tmp);
        }
    }

    /* ── Clean up ─────────────────────────────────────────────────────── */
    forge_raster_buffer_destroy(&fb);
    for (int i = 0; i < 3; i++) forge_ui_atlas_free(&atlases[i]);
    forge_ui_ttf_free(&font);

    SDL_Log("%s", SEPARATOR);
    SDL_Log("Done. 2 frames written.");
    SDL_Log("%s", SEPARATOR);

    SDL_Quit();
    return 0;
}
