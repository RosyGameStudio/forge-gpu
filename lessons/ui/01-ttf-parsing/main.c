/*
 * UI Lesson 01 -- TTF Parsing
 *
 * Demonstrates: Loading a TrueType font file, reading the table directory,
 * extracting font metrics, mapping Unicode codepoints to glyph indices,
 * and parsing glyph outlines from the glyf table.
 *
 * This is a console program -- no GPU or window is needed.  It prints the
 * internal structure of a TTF file so you can see how fonts are organized.
 *
 * Usage:
 *   01-ttf-parsing [path/to/font.ttf]
 *
 * If no argument is given, it looks for the bundled Liberation Mono font
 * at the default asset path.
 *
 * SPDX-License-Identifier: Zlib
 */

#include <SDL3/SDL.h>
#include "ui/forge_ui.h"

/* ── Default font path ──────────────────────────────────────────────────── */
/* Relative to the repository root.  Works when running from the build
 * directory via `python scripts/run.py ui/01`. */
#define DEFAULT_FONT_PATH "assets/fonts/liberation_mono/LiberationMono-Regular.ttf"

/* ── Section separator for console output ────────────────────────────────── */
#define SEPARATOR "============================================================"
#define THIN_SEP  "------------------------------------------------------------"

/* ── Glyph preview constants ────────────────────────────────────────────── */
#define MAX_PREVIEW_POINTS 10   /* max points to print in detail view */
#define ON_CURVE_FLAG      0x01 /* bit 0 of glyph flags = on-curve point */

/* ── Rasterization parameters (visual proof that parsing works) ─────────── */
#define PIXEL_HEIGHT 64.0f       /* render at 64 pixels tall */
#define SS_LEVEL     4           /* 4x4 supersampling for anti-aliasing */

/* ── BMP file writing ────────────────────────────────────────────────────── */
/* Write a single-channel grayscale bitmap as a BMP file.
 *
 * BMP format stores pixels bottom-up (row 0 = bottom of image) with each
 * row padded to a 4-byte boundary.  We write an 8-bit indexed BMP with a
 * 256-entry grayscale palette (0=black, 255=white). */

#define BMP_HEADER_SIZE   14     /* BITMAPFILEHEADER size */
#define BMP_INFO_SIZE     40     /* BITMAPINFOHEADER size */
#define BMP_PALETTE_SIZE  1024   /* 256 entries * 4 bytes (BGRA) */

static bool write_grayscale_bmp(const char *path,
                                 const Uint8 *pixels,
                                 int width, int height)
{
    /* Each row must be padded to a 4-byte boundary */
    int row_stride = (width + 3) & ~3;
    Uint32 pixel_data_size = (Uint32)(row_stride * height);
    Uint32 file_size = BMP_HEADER_SIZE + BMP_INFO_SIZE +
                       BMP_PALETTE_SIZE + pixel_data_size;

    Uint8 *buf = (Uint8 *)SDL_calloc(file_size, 1);
    if (!buf) {
        SDL_Log("write_grayscale_bmp: allocation failed");
        return false;
    }

    /* BITMAPFILEHEADER (14 bytes) */
    buf[0] = 'B'; buf[1] = 'M';
    buf[2] = (Uint8)(file_size);
    buf[3] = (Uint8)(file_size >> 8);
    buf[4] = (Uint8)(file_size >> 16);
    buf[5] = (Uint8)(file_size >> 24);
    Uint32 data_offset = BMP_HEADER_SIZE + BMP_INFO_SIZE + BMP_PALETTE_SIZE;
    buf[10] = (Uint8)(data_offset);
    buf[11] = (Uint8)(data_offset >> 8);
    buf[12] = (Uint8)(data_offset >> 16);
    buf[13] = (Uint8)(data_offset >> 24);

    /* BITMAPINFOHEADER (40 bytes) */
    Uint8 *info = buf + BMP_HEADER_SIZE;
    info[0] = BMP_INFO_SIZE;
    info[4] = (Uint8)(width);
    info[5] = (Uint8)(width >> 8);
    info[6] = (Uint8)(width >> 16);
    info[7] = (Uint8)(width >> 24);
    info[8]  = (Uint8)(height);
    info[9]  = (Uint8)(height >> 8);
    info[10] = (Uint8)(height >> 16);
    info[11] = (Uint8)(height >> 24);
    info[12] = 1;                                /* planes = 1 */
    info[14] = 8;                                /* bits per pixel = 8 */
    info[20] = (Uint8)(pixel_data_size);
    info[21] = (Uint8)(pixel_data_size >> 8);
    info[22] = (Uint8)(pixel_data_size >> 16);
    info[23] = (Uint8)(pixel_data_size >> 24);

    /* Grayscale palette: 256 entries, each (B, G, R, 0) */
    Uint8 *palette = buf + BMP_HEADER_SIZE + BMP_INFO_SIZE;
    for (int i = 0; i < 256; i++) {
        palette[i * 4 + 0] = (Uint8)i;
        palette[i * 4 + 1] = (Uint8)i;
        palette[i * 4 + 2] = (Uint8)i;
        palette[i * 4 + 3] = 0;
    }

    /* Pixel data: BMP stores rows bottom-up, so flip vertically */
    Uint8 *pixel_dst = buf + data_offset;
    for (int y = 0; y < height; y++) {
        int bmp_row = height - 1 - y;
        SDL_memcpy(&pixel_dst[bmp_row * row_stride],
                   &pixels[y * width],
                   (size_t)width);
    }

    FILE *fp = fopen(path, "wb");
    if (!fp) {
        SDL_Log("write_grayscale_bmp: failed to open '%s' for writing", path);
        SDL_free(buf);
        return false;
    }
    size_t written = fwrite(buf, 1, file_size, fp);
    fclose(fp);
    SDL_free(buf);

    if (written != file_size) {
        SDL_Log("write_grayscale_bmp: incomplete write to '%s' "
                "(%zu of %u bytes)", path, written, file_size);
        return false;
    }
    return true;
}

int main(int argc, char *argv[])
{
    /* Use the command-line argument if provided, otherwise fall back to
     * the default path.  This lets users try their own fonts easily. */
    const char *font_path = (argc > 1) ? argv[1] : DEFAULT_FONT_PATH;

    /* SDL_Init(0) initializes the base SDL library without any subsystems.
     * We only need SDL for file I/O and logging -- no video or audio. */
    if (!SDL_Init(0)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    SDL_Log("Loading font: %s", font_path);
    SDL_Log("%s", SEPARATOR);

    /* ── Load the font ───────────────────────────────────────────────── */
    ForgeUiFont font;
    if (!forge_ui_ttf_load(font_path, &font)) {
        SDL_Log("Failed to load font -- see errors above");
        SDL_Quit();
        return 1;
    }

    /* ── Print table directory ───────────────────────────────────────── */
    SDL_Log("TABLE DIRECTORY (%u tables)", font.num_tables);
    SDL_Log("%s", THIN_SEP);
    SDL_Log("  %-6s %10s %10s", "Tag", "Offset", "Length");
    SDL_Log("  %-6s %10s %10s", "----", "------", "------");

    for (Uint16 i = 0; i < font.num_tables; i++) {
        SDL_Log("  %-6s %10u %10u",
                font.tables[i].tag,
                font.tables[i].offset,
                font.tables[i].length);
    }

    /* ── Print head table metrics ────────────────────────────────────── */
    SDL_Log("%s", SEPARATOR);
    SDL_Log("HEAD TABLE");
    SDL_Log("%s", THIN_SEP);
    SDL_Log("  unitsPerEm:      %u", font.head.units_per_em);
    SDL_Log("  bounding box:    (%d, %d) to (%d, %d)",
            font.head.x_min, font.head.y_min,
            font.head.x_max, font.head.y_max);
    SDL_Log("  indexToLocFormat: %d (%s)",
            font.head.index_to_loc_fmt,
            font.head.index_to_loc_fmt == 0 ? "short" : "long");

    /* ── Print hhea table metrics ────────────────────────────────────── */
    SDL_Log("%s", SEPARATOR);
    SDL_Log("HHEA TABLE");
    SDL_Log("%s", THIN_SEP);
    SDL_Log("  ascender:          %d", font.hhea.ascender);
    SDL_Log("  descender:         %d", font.hhea.descender);
    SDL_Log("  lineGap:           %d", font.hhea.line_gap);
    SDL_Log("  numberOfHMetrics:  %u", font.hhea.number_of_h_metrics);

    /* ── Print maxp glyph count ──────────────────────────────────────── */
    SDL_Log("%s", SEPARATOR);
    SDL_Log("MAXP TABLE");
    SDL_Log("%s", THIN_SEP);
    SDL_Log("  numGlyphs: %u", font.maxp.num_glyphs);

    /* ── Look up glyph indices via cmap ──────────────────────────────── */
    SDL_Log("%s", SEPARATOR);
    SDL_Log("CMAP LOOKUPS");
    SDL_Log("%s", THIN_SEP);

    /* Characters to look up -- a mix of upper/lowercase, punctuation,
     * and whitespace to show different cmap segment behavior. */
    const char *test_chars = "Ag! ";

    for (int i = 0; test_chars[i] != '\0'; i++) {
        Uint32 cp = (Uint32)(unsigned char)test_chars[i];
        Uint16 glyph_idx = forge_ui_ttf_glyph_index(&font, cp);

        /* Format the character name for readable output */
        if (cp == ' ') {
            SDL_Log("  '%s' (U+%04X) -> glyph %u", "space", cp, glyph_idx);
        } else {
            SDL_Log("  '%c'     (U+%04X) -> glyph %u",
                    (char)cp, cp, glyph_idx);
        }
    }

    /* ── Load and inspect glyph 'A' ──────────────────────────────────── */
    SDL_Log("%s", SEPARATOR);
    SDL_Log("GLYPH DETAIL: 'A'");
    SDL_Log("%s", THIN_SEP);

    Uint16 a_index = forge_ui_ttf_glyph_index(&font, 'A');
    ForgeUiTtfGlyph glyph_a;

    if (forge_ui_ttf_load_glyph(&font, a_index, &glyph_a)) {
        SDL_Log("  glyph index:   %u", a_index);
        SDL_Log("  bounding box:  (%d, %d) to (%d, %d)",
                glyph_a.x_min, glyph_a.y_min,
                glyph_a.x_max, glyph_a.y_max);
        SDL_Log("  contours:      %u", glyph_a.contour_count);
        SDL_Log("  points:        %u", glyph_a.point_count);

        /* Print contour endpoints */
        if (glyph_a.contour_count > 0) {
            SDL_Log("  contour ends:  ");
            for (Uint16 c = 0; c < glyph_a.contour_count; c++) {
                SDL_Log("    contour %u ends at point %u", c,
                        glyph_a.contour_ends[c]);
            }
        }

        /* Print first few points to show the coordinate data */
        Uint16 show_count = glyph_a.point_count;
        if (show_count > MAX_PREVIEW_POINTS) show_count = MAX_PREVIEW_POINTS;

        SDL_Log("  first %u points:", show_count);
        for (Uint16 i = 0; i < show_count; i++) {
            bool on_curve = (glyph_a.flags[i] & ON_CURVE_FLAG) != 0;
            SDL_Log("    [%2u] (%5d, %5d) %s",
                    i,
                    glyph_a.points[i].x,
                    glyph_a.points[i].y,
                    on_curve ? "on-curve" : "off-curve");
        }
        if (glyph_a.point_count > show_count) {
            SDL_Log("    ... (%u more points)", glyph_a.point_count - show_count);
        }

        forge_ui_ttf_glyph_free(&glyph_a);
    } else {
        SDL_Log("  Failed to load glyph 'A'");
    }

    /* ── Visual proof: rasterize 'A' to a BMP ─────────────────────────── */
    /* Rasterize the glyph we just inspected to show that the parsing
     * produced valid outline data.  This is a preview of what
     * UI Lesson 02 (Glyph Rasterization) covers in depth. */
    SDL_Log("%s", SEPARATOR);
    SDL_Log("VISUAL PROOF: Rasterize 'A' to BMP");
    SDL_Log("%s", THIN_SEP);

    ForgeUiRasterOpts opts;
    opts.supersample_level = SS_LEVEL;

    ForgeUiGlyphBitmap bitmap;
    if (forge_ui_rasterize_glyph(&font, a_index, PIXEL_HEIGHT,
                                  &opts, &bitmap)) {
        SDL_Log("  bitmap size:   %d x %d pixels", bitmap.width, bitmap.height);
        SDL_Log("  bearing:       (%d, %d)", bitmap.bearing_x, bitmap.bearing_y);

        if (write_grayscale_bmp("glyph_A.bmp", bitmap.pixels,
                                 bitmap.width, bitmap.height)) {
            SDL_Log("  saved:         glyph_A.bmp");
        }
        forge_ui_glyph_bitmap_free(&bitmap);
    } else {
        SDL_Log("  Failed to rasterize 'A'");
    }

    SDL_Log("%s", SEPARATOR);
    SDL_Log("Done. BMP file written to the current directory.");

    /* ── Cleanup ─────────────────────────────────────────────────────── */
    forge_ui_ttf_free(&font);
    SDL_Quit();
    return 0;
}
