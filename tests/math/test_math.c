/*
 * Math Library Tests
 *
 * Automated tests for common/math/forge_math.h
 * Verifies correctness of all vector and matrix operations.
 *
 * Exit code: 0 if all tests pass, 1 if any test fails
 *
 * SPDX-License-Identifier: Zlib
 */

#include <SDL3/SDL.h>
#include "math/forge_math.h"

/* ── Test Framework ──────────────────────────────────────────────────────── */

static int test_count = 0;
static int pass_count = 0;
static int fail_count = 0;

/* Epsilon for floating-point comparisons (account for rounding errors) */
#define EPSILON 0.0001f

/* Check if two floats are approximately equal */
static bool float_eq(float a, float b)
{
    return SDL_fabsf(a - b) < EPSILON;
}

/* Check if two vec2s are approximately equal */
static bool vec2_eq(vec2 a, vec2 b)
{
    return float_eq(a.x, b.x) && float_eq(a.y, b.y);
}

/* Check if two vec3s are approximately equal */
static bool vec3_eq(vec3 a, vec3 b)
{
    return float_eq(a.x, b.x) && float_eq(a.y, b.y) && float_eq(a.z, b.z);
}

/* Check if two vec4s are approximately equal */
static bool vec4_eq(vec4 a, vec4 b)
{
    return float_eq(a.x, b.x) && float_eq(a.y, b.y) &&
           float_eq(a.z, b.z) && float_eq(a.w, b.w);
}

/* Test assertion macros */
#define TEST(name) \
    do { \
        test_count++; \
        SDL_Log("  Testing: %s", name);

#define ASSERT_FLOAT_EQ(a, b) \
    if (!float_eq(a, b)) { \
        SDL_LogError(SDL_LOG_CATEGORY_TEST, \
                     "    FAIL: Expected %.6f, got %.6f", b, a); \
        fail_count++; \
        return; \
    }

#define ASSERT_VEC2_EQ(a, b) \
    if (!vec2_eq(a, b)) { \
        SDL_LogError(SDL_LOG_CATEGORY_TEST, \
                     "    FAIL: Expected (%.3f, %.3f), got (%.3f, %.3f)", \
                     b.x, b.y, a.x, a.y); \
        fail_count++; \
        return; \
    }

#define ASSERT_VEC3_EQ(a, b) \
    if (!vec3_eq(a, b)) { \
        SDL_LogError(SDL_LOG_CATEGORY_TEST, \
                     "    FAIL: Expected (%.3f, %.3f, %.3f), got (%.3f, %.3f, %.3f)", \
                     b.x, b.y, b.z, a.x, a.y, a.z); \
        fail_count++; \
        return; \
    }

#define ASSERT_VEC4_EQ(a, b) \
    if (!vec4_eq(a, b)) { \
        SDL_LogError(SDL_LOG_CATEGORY_TEST, \
                     "    FAIL: Expected (%.3f, %.3f, %.3f, %.3f), got (%.3f, %.3f, %.3f, %.3f)", \
                     b.x, b.y, b.z, b.w, a.x, a.y, a.z, a.w); \
        fail_count++; \
        return; \
    }

#define END_TEST() \
        SDL_Log("    PASS"); \
        pass_count++; \
    } while (0)

/* ══════════════════════════════════════════════════════════════════════════
 * vec2 Tests
 * ══════════════════════════════════════════════════════════════════════════ */

static void test_vec2_create(void)
{
    TEST("vec2_create");
    vec2 v = vec2_create(1.0f, 2.0f);
    ASSERT_FLOAT_EQ(v.x, 1.0f);
    ASSERT_FLOAT_EQ(v.y, 2.0f);
    END_TEST();
}

static void test_vec2_add(void)
{
    TEST("vec2_add");
    vec2 a = vec2_create(1.0f, 2.0f);
    vec2 b = vec2_create(3.0f, 4.0f);
    vec2 result = vec2_add(a, b);
    ASSERT_VEC2_EQ(result, vec2_create(4.0f, 6.0f));
    END_TEST();
}

static void test_vec2_sub(void)
{
    TEST("vec2_sub");
    vec2 a = vec2_create(5.0f, 3.0f);
    vec2 b = vec2_create(2.0f, 1.0f);
    vec2 result = vec2_sub(a, b);
    ASSERT_VEC2_EQ(result, vec2_create(3.0f, 2.0f));
    END_TEST();
}

static void test_vec2_scale(void)
{
    TEST("vec2_scale");
    vec2 v = vec2_create(2.0f, 3.0f);
    vec2 result = vec2_scale(v, 2.0f);
    ASSERT_VEC2_EQ(result, vec2_create(4.0f, 6.0f));
    END_TEST();
}

static void test_vec2_dot(void)
{
    TEST("vec2_dot");
    vec2 a = vec2_create(1.0f, 0.0f);
    vec2 b = vec2_create(0.0f, 1.0f);
    float dot = vec2_dot(a, b);
    ASSERT_FLOAT_EQ(dot, 0.0f);  /* Perpendicular */

    vec2 c = vec2_create(2.0f, 0.0f);
    float dot2 = vec2_dot(a, c);
    ASSERT_FLOAT_EQ(dot2, 2.0f);  /* Parallel */
    END_TEST();
}

static void test_vec2_length(void)
{
    TEST("vec2_length");
    vec2 v = vec2_create(3.0f, 4.0f);
    float len = vec2_length(v);
    ASSERT_FLOAT_EQ(len, 5.0f);  /* 3-4-5 triangle */
    END_TEST();
}

static void test_vec2_normalize(void)
{
    TEST("vec2_normalize");
    vec2 v = vec2_create(3.0f, 4.0f);
    vec2 normalized = vec2_normalize(v);
    ASSERT_FLOAT_EQ(vec2_length(normalized), 1.0f);  /* Unit length */
    ASSERT_VEC2_EQ(normalized, vec2_create(0.6f, 0.8f));
    END_TEST();
}

static void test_vec2_lerp(void)
{
    TEST("vec2_lerp");
    vec2 a = vec2_create(0.0f, 0.0f);
    vec2 b = vec2_create(10.0f, 10.0f);
    vec2 mid = vec2_lerp(a, b, 0.5f);
    ASSERT_VEC2_EQ(mid, vec2_create(5.0f, 5.0f));
    END_TEST();
}

/* ══════════════════════════════════════════════════════════════════════════
 * vec3 Tests
 * ══════════════════════════════════════════════════════════════════════════ */

static void test_vec3_create(void)
{
    TEST("vec3_create");
    vec3 v = vec3_create(1.0f, 2.0f, 3.0f);
    ASSERT_FLOAT_EQ(v.x, 1.0f);
    ASSERT_FLOAT_EQ(v.y, 2.0f);
    ASSERT_FLOAT_EQ(v.z, 3.0f);
    END_TEST();
}

static void test_vec3_add(void)
{
    TEST("vec3_add");
    vec3 a = vec3_create(1.0f, 2.0f, 3.0f);
    vec3 b = vec3_create(4.0f, 5.0f, 6.0f);
    vec3 result = vec3_add(a, b);
    ASSERT_VEC3_EQ(result, vec3_create(5.0f, 7.0f, 9.0f));
    END_TEST();
}

static void test_vec3_sub(void)
{
    TEST("vec3_sub");
    vec3 a = vec3_create(5.0f, 3.0f, 2.0f);
    vec3 b = vec3_create(2.0f, 1.0f, 1.0f);
    vec3 result = vec3_sub(a, b);
    ASSERT_VEC3_EQ(result, vec3_create(3.0f, 2.0f, 1.0f));
    END_TEST();
}

static void test_vec3_scale(void)
{
    TEST("vec3_scale");
    vec3 v = vec3_create(1.0f, 2.0f, 3.0f);
    vec3 result = vec3_scale(v, 2.0f);
    ASSERT_VEC3_EQ(result, vec3_create(2.0f, 4.0f, 6.0f));
    END_TEST();
}

static void test_vec3_dot(void)
{
    TEST("vec3_dot");
    vec3 a = vec3_create(1.0f, 0.0f, 0.0f);
    vec3 b = vec3_create(0.0f, 1.0f, 0.0f);
    float dot = vec3_dot(a, b);
    ASSERT_FLOAT_EQ(dot, 0.0f);  /* Perpendicular */
    END_TEST();
}

static void test_vec3_cross(void)
{
    TEST("vec3_cross");
    vec3 x = vec3_create(1.0f, 0.0f, 0.0f);
    vec3 y = vec3_create(0.0f, 1.0f, 0.0f);
    vec3 z = vec3_cross(x, y);
    ASSERT_VEC3_EQ(z, vec3_create(0.0f, 0.0f, 1.0f));  /* X × Y = Z */

    /* Verify perpendicularity */
    ASSERT_FLOAT_EQ(vec3_dot(z, x), 0.0f);
    ASSERT_FLOAT_EQ(vec3_dot(z, y), 0.0f);
    END_TEST();
}

static void test_vec3_length(void)
{
    TEST("vec3_length");
    vec3 v = vec3_create(3.0f, 4.0f, 0.0f);
    float len = vec3_length(v);
    ASSERT_FLOAT_EQ(len, 5.0f);
    END_TEST();
}

static void test_vec3_normalize(void)
{
    TEST("vec3_normalize");
    vec3 v = vec3_create(3.0f, 4.0f, 0.0f);
    vec3 normalized = vec3_normalize(v);
    ASSERT_FLOAT_EQ(vec3_length(normalized), 1.0f);
    END_TEST();
}

static void test_vec3_lerp(void)
{
    TEST("vec3_lerp");
    vec3 a = vec3_create(0.0f, 0.0f, 0.0f);
    vec3 b = vec3_create(10.0f, 10.0f, 10.0f);
    vec3 mid = vec3_lerp(a, b, 0.5f);
    ASSERT_VEC3_EQ(mid, vec3_create(5.0f, 5.0f, 5.0f));
    END_TEST();
}

/* ══════════════════════════════════════════════════════════════════════════
 * vec4 Tests
 * ══════════════════════════════════════════════════════════════════════════ */

static void test_vec4_create(void)
{
    TEST("vec4_create");
    vec4 v = vec4_create(1.0f, 2.0f, 3.0f, 4.0f);
    ASSERT_FLOAT_EQ(v.x, 1.0f);
    ASSERT_FLOAT_EQ(v.y, 2.0f);
    ASSERT_FLOAT_EQ(v.z, 3.0f);
    ASSERT_FLOAT_EQ(v.w, 4.0f);
    END_TEST();
}

static void test_vec4_add(void)
{
    TEST("vec4_add");
    vec4 a = vec4_create(1.0f, 2.0f, 3.0f, 4.0f);
    vec4 b = vec4_create(5.0f, 6.0f, 7.0f, 8.0f);
    vec4 result = vec4_add(a, b);
    ASSERT_VEC4_EQ(result, vec4_create(6.0f, 8.0f, 10.0f, 12.0f));
    END_TEST();
}

static void test_vec4_dot(void)
{
    TEST("vec4_dot");
    vec4 a = vec4_create(1.0f, 0.0f, 0.0f, 0.0f);
    vec4 b = vec4_create(0.0f, 1.0f, 0.0f, 0.0f);
    float dot = vec4_dot(a, b);
    ASSERT_FLOAT_EQ(dot, 0.0f);
    END_TEST();
}

/* ══════════════════════════════════════════════════════════════════════════
 * mat4 Tests
 * ══════════════════════════════════════════════════════════════════════════ */

static void test_mat4_identity(void)
{
    TEST("mat4_identity");
    mat4 m = mat4_identity();

    /* Diagonal should be 1.0 */
    ASSERT_FLOAT_EQ(m.m[0], 1.0f);
    ASSERT_FLOAT_EQ(m.m[5], 1.0f);
    ASSERT_FLOAT_EQ(m.m[10], 1.0f);
    ASSERT_FLOAT_EQ(m.m[15], 1.0f);

    /* Off-diagonal should be 0.0 */
    ASSERT_FLOAT_EQ(m.m[1], 0.0f);
    ASSERT_FLOAT_EQ(m.m[4], 0.0f);
    END_TEST();
}

static void test_mat4_translate(void)
{
    TEST("mat4_translate");
    mat4 m = mat4_translate(vec3_create(5.0f, 3.0f, 2.0f));

    /* Translation is in column 3 (indices 12, 13, 14) */
    ASSERT_FLOAT_EQ(m.m[12], 5.0f);
    ASSERT_FLOAT_EQ(m.m[13], 3.0f);
    ASSERT_FLOAT_EQ(m.m[14], 2.0f);

    /* Transform a point */
    vec4 point = vec4_create(0.0f, 0.0f, 0.0f, 1.0f);
    vec4 result = mat4_multiply_vec4(m, point);
    ASSERT_VEC4_EQ(result, vec4_create(5.0f, 3.0f, 2.0f, 1.0f));
    END_TEST();
}

static void test_mat4_scale(void)
{
    TEST("mat4_scale");
    mat4 m = mat4_scale(vec3_create(2.0f, 3.0f, 4.0f));

    vec4 v = vec4_create(1.0f, 1.0f, 1.0f, 1.0f);
    vec4 result = mat4_multiply_vec4(m, v);
    ASSERT_VEC4_EQ(result, vec4_create(2.0f, 3.0f, 4.0f, 1.0f));
    END_TEST();
}

static void test_mat4_rotate_z(void)
{
    TEST("mat4_rotate_z");
    /* 90-degree rotation around Z should turn X-axis into Y-axis */
    mat4 m = mat4_rotate_z(FORGE_PI / 2.0f);
    vec4 x_axis = vec4_create(1.0f, 0.0f, 0.0f, 0.0f);
    vec4 result = mat4_multiply_vec4(m, x_axis);

    /* Should be approximately (0, 1, 0, 0) */
    ASSERT_FLOAT_EQ(result.x, 0.0f);
    ASSERT_FLOAT_EQ(result.y, 1.0f);
    ASSERT_FLOAT_EQ(result.z, 0.0f);
    END_TEST();
}

static void test_mat4_multiply(void)
{
    TEST("mat4_multiply");
    /* Translate then scale should scale first, then translate */
    mat4 translate = mat4_translate(vec3_create(10.0f, 0.0f, 0.0f));
    mat4 scale = mat4_scale_uniform(2.0f);
    mat4 combined = mat4_multiply(translate, scale);

    vec4 point = vec4_create(1.0f, 0.0f, 0.0f, 1.0f);
    vec4 result = mat4_multiply_vec4(combined, point);

    /* Scale first (1 * 2 = 2), then translate (2 + 10 = 12) */
    ASSERT_FLOAT_EQ(result.x, 12.0f);
    END_TEST();
}

static void test_mat4_multiply_identity(void)
{
    TEST("mat4_multiply with identity");
    mat4 m = mat4_translate(vec3_create(5.0f, 3.0f, 2.0f));
    mat4 identity = mat4_identity();
    mat4 result = mat4_multiply(m, identity);

    /* Should equal the original matrix */
    for (int i = 0; i < 16; i++) {
        ASSERT_FLOAT_EQ(result.m[i], m.m[i]);
    }
    END_TEST();
}

/* ══════════════════════════════════════════════════════════════════════════
 * Main
 * ══════════════════════════════════════════════════════════════════════════ */

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    SDL_Log("\n=== forge-gpu Math Library Tests ===\n");

    /* vec2 tests */
    SDL_Log("vec2 tests:");
    test_vec2_create();
    test_vec2_add();
    test_vec2_sub();
    test_vec2_scale();
    test_vec2_dot();
    test_vec2_length();
    test_vec2_normalize();
    test_vec2_lerp();

    /* vec3 tests */
    SDL_Log("\nvec3 tests:");
    test_vec3_create();
    test_vec3_add();
    test_vec3_sub();
    test_vec3_scale();
    test_vec3_dot();
    test_vec3_cross();
    test_vec3_length();
    test_vec3_normalize();
    test_vec3_lerp();

    /* vec4 tests */
    SDL_Log("\nvec4 tests:");
    test_vec4_create();
    test_vec4_add();
    test_vec4_dot();

    /* mat4 tests */
    SDL_Log("\nmat4 tests:");
    test_mat4_identity();
    test_mat4_translate();
    test_mat4_scale();
    test_mat4_rotate_z();
    test_mat4_multiply();
    test_mat4_multiply_identity();

    /* Summary */
    SDL_Log("\n=== Test Summary ===");
    SDL_Log("Total:  %d", test_count);
    SDL_Log("Passed: %d", pass_count);
    SDL_Log("Failed: %d", fail_count);

    if (fail_count > 0) {
        SDL_LogError(SDL_LOG_CATEGORY_TEST, "\nSome tests FAILED!");
        SDL_Quit();
        return 1;
    }

    SDL_Log("\nAll tests PASSED!");
    SDL_Quit();
    return 0;
}
