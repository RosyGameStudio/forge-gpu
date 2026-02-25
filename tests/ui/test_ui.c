/*
 * UI Parser Tests
 *
 * Automated tests for common/ui/forge_ui.h TTF parser.
 * Verifies correctness of font loading, table directory parsing,
 * metric extraction, cmap lookups, and glyph outline parsing.
 *
 * Uses the bundled Liberation Mono Regular font for all tests.
 *
 * Exit code: 0 if all tests pass, 1 if any test fails
 *
 * SPDX-License-Identifier: Zlib
 */

#include <SDL3/SDL.h>
#include "ui/forge_ui.h"

/* ── Test Framework ──────────────────────────────────────────────────────── */

static int test_count = 0;
static int pass_count = 0;
static int fail_count = 0;

#define TEST(name)                                                \
    do {                                                          \
        test_count++;                                             \
        SDL_Log("  [TEST] %s", name);                             \
    } while (0)

#define ASSERT_TRUE(expr)                                         \
    do {                                                          \
        if (!(expr)) {                                            \
            SDL_Log("    FAIL: %s (line %d)", #expr, __LINE__);   \
            fail_count++;                                         \
            return;                                               \
        }                                                         \
        pass_count++;                                             \
    } while (0)

#define ASSERT_EQ_U16(a, b)                                       \
    do {                                                          \
        Uint16 _a = (a), _b = (b);                                \
        if (_a != _b) {                                           \
            SDL_Log("    FAIL: %s == %u, expected %u (line %d)",  \
                    #a, _a, _b, __LINE__);                        \
            fail_count++;                                         \
            return;                                               \
        }                                                         \
        pass_count++;                                             \
    } while (0)

#define ASSERT_EQ_U32(a, b)                                       \
    do {                                                          \
        Uint32 _a = (a), _b = (b);                                \
        if (_a != _b) {                                           \
            SDL_Log("    FAIL: %s == %u, expected %u (line %d)",  \
                    #a, _a, _b, __LINE__);                        \
            fail_count++;                                         \
            return;                                               \
        }                                                         \
        pass_count++;                                             \
    } while (0)

#define ASSERT_EQ_I16(a, b)                                       \
    do {                                                          \
        Sint16 _a = (a), _b = (b);                                \
        if (_a != _b) {                                           \
            SDL_Log("    FAIL: %s == %d, expected %d (line %d)",  \
                    #a, _a, _b, __LINE__);                        \
            fail_count++;                                         \
            return;                                               \
        }                                                         \
        pass_count++;                                             \
    } while (0)

/* ── Test font path ──────────────────────────────────────────────────────── */

#define TEST_FONT_PATH "assets/fonts/liberation_mono/LiberationMono-Regular.ttf"

/* Shared font instance loaded once for all tests */
static ForgeUiFont test_font;
static bool font_loaded = false;

/* ── Test: font loading ──────────────────────────────────────────────────── */

static void test_font_load(void)
{
    TEST("forge_ui_ttf_load succeeds with valid font");
    font_loaded = forge_ui_ttf_load(TEST_FONT_PATH, &test_font);
    ASSERT_TRUE(font_loaded);
}

static void test_font_load_nonexistent(void)
{
    TEST("forge_ui_ttf_load fails with nonexistent path");
    ForgeUiFont bad_font;
    bool result = forge_ui_ttf_load("nonexistent.ttf", &bad_font);
    ASSERT_TRUE(!result);
}

/* ── Test: table directory ───────────────────────────────────────────────── */

static void test_table_directory(void)
{
    TEST("table directory has expected count");
    if (!font_loaded) return;

    /* Liberation Mono Regular has 16 tables */
    ASSERT_EQ_U16(test_font.num_tables, 16);
}

static void test_table_lookup(void)
{
    TEST("required tables are present");
    if (!font_loaded) return;

    /* All required tables must be found */
    const char *required[] = {"head", "hhea", "maxp", "cmap", "loca", "glyf"};
    for (int i = 0; i < 6; i++) {
        const ForgeUiTtfTableEntry *t = forge_ui__find_table(
            &test_font, required[i]);
        if (!t) {
            SDL_Log("    FAIL: table '%s' not found (line %d)",
                    required[i], __LINE__);
            fail_count++;
            return;
        }
    }
    pass_count++;
}

static void test_table_entry_bounds(void)
{
    TEST("table entries have valid offset+length within file");
    if (!font_loaded) return;

    for (Uint16 i = 0; i < test_font.num_tables; i++) {
        Uint64 end = (Uint64)test_font.tables[i].offset +
                     (Uint64)test_font.tables[i].length;
        if (end > test_font.data_size) {
            SDL_Log("    FAIL: table '%.4s' offset+length (%u+%u) exceeds "
                    "file size (%zu) (line %d)",
                    test_font.tables[i].tag,
                    test_font.tables[i].offset,
                    test_font.tables[i].length,
                    test_font.data_size, __LINE__);
            fail_count++;
            return;
        }
    }
    pass_count++;
}

/* ── Test: head table ────────────────────────────────────────────────────── */

static void test_head_units_per_em(void)
{
    TEST("head: unitsPerEm is 2048");
    if (!font_loaded) return;
    ASSERT_EQ_U16(test_font.head.units_per_em, 2048);
}

static void test_head_index_to_loc_format(void)
{
    TEST("head: indexToLocFormat is 0 (short)");
    if (!font_loaded) return;
    ASSERT_EQ_I16(test_font.head.index_to_loc_fmt, 0);
}

static void test_head_bounding_box(void)
{
    TEST("head: global bounding box values");
    if (!font_loaded) return;
    ASSERT_EQ_I16(test_font.head.x_min, -50);
    ASSERT_EQ_I16(test_font.head.y_min, -615);
    ASSERT_EQ_I16(test_font.head.x_max, 1247);
    ASSERT_EQ_I16(test_font.head.y_max, 1705);
}

/* ── Test: hhea table ────────────────────────────────────────────────────── */

static void test_hhea_metrics(void)
{
    TEST("hhea: ascender, descender, lineGap");
    if (!font_loaded) return;
    ASSERT_EQ_I16(test_font.hhea.ascender, 1705);
    ASSERT_EQ_I16(test_font.hhea.descender, -615);
    ASSERT_EQ_I16(test_font.hhea.line_gap, 0);
}

static void test_hhea_num_hmetrics(void)
{
    TEST("hhea: numberOfHMetrics");
    if (!font_loaded) return;
    ASSERT_EQ_U16(test_font.hhea.number_of_h_metrics, 4);
}

/* ── Test: maxp table ────────────────────────────────────────────────────── */

static void test_maxp_num_glyphs(void)
{
    TEST("maxp: numGlyphs is 670");
    if (!font_loaded) return;
    ASSERT_EQ_U16(test_font.maxp.num_glyphs, 670);
}

/* ── Test: cmap lookups ──────────────────────────────────────────────────── */

static void test_cmap_ascii_a(void)
{
    TEST("cmap: 'A' (U+0041) maps to glyph 36");
    if (!font_loaded) return;
    ASSERT_EQ_U16(forge_ui_ttf_glyph_index(&test_font, 'A'), 36);
}

static void test_cmap_ascii_g(void)
{
    TEST("cmap: 'g' (U+0067) maps to glyph 74");
    if (!font_loaded) return;
    ASSERT_EQ_U16(forge_ui_ttf_glyph_index(&test_font, 'g'), 74);
}

static void test_cmap_space(void)
{
    TEST("cmap: space (U+0020) maps to glyph 3");
    if (!font_loaded) return;
    ASSERT_EQ_U16(forge_ui_ttf_glyph_index(&test_font, ' '), 3);
}

static void test_cmap_unmapped(void)
{
    TEST("cmap: unmapped codepoint returns 0 (.notdef)");
    if (!font_loaded) return;
    /* U+FFFE is guaranteed to be a noncharacter */
    ASSERT_EQ_U16(forge_ui_ttf_glyph_index(&test_font, 0xFFFE), 0);
}

static void test_cmap_beyond_bmp(void)
{
    TEST("cmap: codepoint > 0xFFFF returns 0 (format 4 BMP only)");
    if (!font_loaded) return;
    ASSERT_EQ_U16(forge_ui_ttf_glyph_index(&test_font, 0x10000), 0);
}

/* ── Test: glyph index out of range ──────────────────────────────────────── */

static void test_glyph_out_of_range(void)
{
    TEST("load_glyph: index >= numGlyphs returns false");
    if (!font_loaded) return;
    ForgeUiTtfGlyph glyph;
    bool result = forge_ui_ttf_load_glyph(
        &test_font, test_font.maxp.num_glyphs, &glyph);
    ASSERT_TRUE(!result);
}

/* ── Test: space glyph (zero-length) ─────────────────────────────────────── */

static void test_glyph_space(void)
{
    TEST("load_glyph: space glyph has no contours");
    if (!font_loaded) return;
    Uint16 idx = forge_ui_ttf_glyph_index(&test_font, ' ');
    ForgeUiTtfGlyph glyph;
    bool result = forge_ui_ttf_load_glyph(&test_font, idx, &glyph);
    ASSERT_TRUE(result);
    ASSERT_EQ_U16(glyph.contour_count, 0);
    ASSERT_EQ_U16(glyph.point_count, 0);
    forge_ui_ttf_glyph_free(&glyph);
}

/* ── Test: glyph 'A' outline ─────────────────────────────────────────────── */

static void test_glyph_a_contours(void)
{
    TEST("load_glyph: 'A' has 2 contours and 21 points");
    if (!font_loaded) return;
    Uint16 idx = forge_ui_ttf_glyph_index(&test_font, 'A');
    ForgeUiTtfGlyph glyph;
    bool result = forge_ui_ttf_load_glyph(&test_font, idx, &glyph);
    ASSERT_TRUE(result);
    ASSERT_EQ_U16(glyph.contour_count, 2);
    ASSERT_EQ_U16(glyph.point_count, 21);
    forge_ui_ttf_glyph_free(&glyph);
}

static void test_glyph_a_bbox(void)
{
    TEST("load_glyph: 'A' bounding box");
    if (!font_loaded) return;
    Uint16 idx = forge_ui_ttf_glyph_index(&test_font, 'A');
    ForgeUiTtfGlyph glyph;
    bool result = forge_ui_ttf_load_glyph(&test_font, idx, &glyph);
    ASSERT_TRUE(result);
    ASSERT_EQ_I16(glyph.x_min, 0);
    ASSERT_EQ_I16(glyph.y_min, 0);
    ASSERT_EQ_I16(glyph.x_max, 1228);
    ASSERT_EQ_I16(glyph.y_max, 1349);
    forge_ui_ttf_glyph_free(&glyph);
}

static void test_glyph_a_contour_ends(void)
{
    TEST("load_glyph: 'A' contour endpoints");
    if (!font_loaded) return;
    Uint16 idx = forge_ui_ttf_glyph_index(&test_font, 'A');
    ForgeUiTtfGlyph glyph;
    bool result = forge_ui_ttf_load_glyph(&test_font, idx, &glyph);
    ASSERT_TRUE(result);
    /* Contour 0 ends at point 7, contour 1 ends at point 20 */
    ASSERT_EQ_U16(glyph.contour_ends[0], 7);
    ASSERT_EQ_U16(glyph.contour_ends[1], 20);
    forge_ui_ttf_glyph_free(&glyph);
}

static void test_glyph_a_first_point(void)
{
    TEST("load_glyph: 'A' first point coordinates");
    if (!font_loaded) return;
    Uint16 idx = forge_ui_ttf_glyph_index(&test_font, 'A');
    ForgeUiTtfGlyph glyph;
    bool result = forge_ui_ttf_load_glyph(&test_font, idx, &glyph);
    ASSERT_TRUE(result);
    /* First point: (1034, 0), on-curve */
    ASSERT_EQ_I16(glyph.points[0].x, 1034);
    ASSERT_EQ_I16(glyph.points[0].y, 0);
    ASSERT_TRUE((glyph.flags[0] & 0x01) != 0); /* on-curve */
    forge_ui_ttf_glyph_free(&glyph);
}

/* ── Test: glyph free is safe on zeroed struct ───────────────────────────── */

static void test_glyph_free_zeroed(void)
{
    TEST("glyph_free: safe on zero-initialized struct");
    ForgeUiTtfGlyph glyph;
    SDL_memset(&glyph, 0, sizeof(glyph));
    forge_ui_ttf_glyph_free(&glyph); /* must not crash */
    pass_count++;
}

/* ── Test: loca offsets are monotonically non-decreasing ─────────────────── */

static void test_loca_monotonic(void)
{
    TEST("loca: offsets are monotonically non-decreasing");
    if (!font_loaded) return;
    Uint32 count = (Uint32)test_font.maxp.num_glyphs + 1;
    for (Uint32 i = 1; i < count; i++) {
        if (test_font.loca_offsets[i] < test_font.loca_offsets[i - 1]) {
            SDL_Log("    FAIL: loca[%u]=%u < loca[%u]=%u (line %d)",
                    i, test_font.loca_offsets[i],
                    i - 1, test_font.loca_offsets[i - 1], __LINE__);
            fail_count++;
            return;
        }
    }
    pass_count++;
}

/* ── Main ────────────────────────────────────────────────────────────────── */

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    if (!SDL_Init(0)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    SDL_Log("=== UI Parser Tests ===");

    /* Font loading */
    test_font_load();
    test_font_load_nonexistent();

    /* Table directory */
    test_table_directory();
    test_table_lookup();
    test_table_entry_bounds();

    /* head table */
    test_head_units_per_em();
    test_head_index_to_loc_format();
    test_head_bounding_box();

    /* hhea table */
    test_hhea_metrics();
    test_hhea_num_hmetrics();

    /* maxp table */
    test_maxp_num_glyphs();

    /* cmap lookups */
    test_cmap_ascii_a();
    test_cmap_ascii_g();
    test_cmap_space();
    test_cmap_unmapped();
    test_cmap_beyond_bmp();

    /* Glyph loading */
    test_glyph_out_of_range();
    test_glyph_space();
    test_glyph_a_contours();
    test_glyph_a_bbox();
    test_glyph_a_contour_ends();
    test_glyph_a_first_point();
    test_glyph_free_zeroed();

    /* loca validation */
    test_loca_monotonic();

    /* Cleanup */
    if (font_loaded) {
        forge_ui_ttf_free(&test_font);
    }
    SDL_Quit();

    SDL_Log("=== Results: %d tests, %d passed, %d failed ===",
            test_count, test_count - fail_count, fail_count);

    return fail_count > 0 ? 1 : 0;
}
