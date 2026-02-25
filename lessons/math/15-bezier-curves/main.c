/*
 * Math Lesson 15 — Bezier Curves
 *
 * Demonstrates quadratic and cubic Bezier curves: evaluation via
 * De Casteljau's algorithm, tangent computation, arc-length approximation,
 * and the relationship between control points and curve shape.
 *
 * This is a console program that prints examples of each operation,
 * building intuition for how Bezier curves work.
 *
 * SPDX-License-Identifier: Zlib
 */

#include <SDL3/SDL.h>
#include "math/forge_math.h"

/* ── Helpers ──────────────────────────────────────────────────────────────── */

static void print_vec2(const char *name, vec2 v)
{
    SDL_Log("%s = (%.4f, %.4f)", name, v.x, v.y);
}

/* Print a short horizontal line of points sampled along a 2D curve. */
static void print_curve_samples(const char *label, vec2 *pts, int count)
{
    SDL_Log("%s (%d samples):", label, count);
    for (int i = 0; i < count; i++) {
        SDL_Log("  t=%.2f  ->  (%.4f, %.4f)",
                (float)i / (float)(count - 1), pts[i].x, pts[i].y);
    }
}

/* ── Main ────────────────────────────────────────────────────────────────── */

#define SAMPLE_COUNT      9      /* Number of samples to show along each curve */
#define DEMO_STEPS        4      /* Number of intervals for demonstration loops */
#define FLOAT_TOLERANCE   0.001f /* Tolerance for floating-point comparison */
#define FLATTEN_MAX_POINTS 512   /* Maximum points for adaptive flattening output */

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    SDL_Log("\n=== Bezier Curves Demo ===\n");

    /* ================================================================== */
    /*  1. Linear interpolation (lerp) — the foundation                   */
    /* ================================================================== */
    SDL_Log("--- 1. Linear Interpolation (Lerp) ---");
    SDL_Log("Bezier curves are built entirely from lerp (linear interpolation).");
    SDL_Log("lerp(a, b, t) = a + t * (b - a)");
    SDL_Log(" ");

    vec2 a = vec2_create(0.0f, 0.0f);
    vec2 b = vec2_create(4.0f, 2.0f);

    SDL_Log("Endpoints:  a = (0, 0),  b = (4, 2)");
    for (int i = 0; i <= DEMO_STEPS; i++) {
        float t = (float)i / (float)DEMO_STEPS;
        vec2 p = vec2_lerp(a, b, t);
        SDL_Log("  lerp(a, b, %.2f) = (%.4f, %.4f)", t, p.x, p.y);
    }
    SDL_Log("A straight line segment IS a degree-1 Bezier curve.");
    SDL_Log(" ");

    /* ================================================================== */
    /*  2. Quadratic Bezier — De Casteljau with 3 control points          */
    /* ================================================================== */
    SDL_Log("--- 2. Quadratic Bezier Curve (3 Control Points) ---");
    SDL_Log("De Casteljau's algorithm: lerp twice to get the curve point.");
    SDL_Log(" ");

    vec2 qp0 = vec2_create(0.0f, 0.0f);  /* Start */
    vec2 qp1 = vec2_create(2.0f, 4.0f);  /* Guide */
    vec2 qp2 = vec2_create(4.0f, 0.0f);  /* End   */

    SDL_Log("Control points:");
    print_vec2("  p0 (start)", qp0);
    print_vec2("  p1 (guide)", qp1);
    print_vec2("  p2 (end)  ", qp2);
    SDL_Log(" ");

    /* Show De Casteljau step by step for t = 0.5 */
    float t_demo = 0.5f;
    SDL_Log("De Casteljau at t = %.1f:", t_demo);

    vec2 q0 = vec2_lerp(qp0, qp1, t_demo);
    vec2 q1 = vec2_lerp(qp1, qp2, t_demo);
    vec2 qr = vec2_lerp(q0, q1, t_demo);

    SDL_Log("  Round 1: q0 = lerp(p0, p1, 0.5) = (%.4f, %.4f)", q0.x, q0.y);
    SDL_Log("  Round 1: q1 = lerp(p1, p2, 0.5) = (%.4f, %.4f)", q1.x, q1.y);
    SDL_Log("  Round 2: result = lerp(q0, q1, 0.5) = (%.4f, %.4f)", qr.x, qr.y);
    SDL_Log(" ");

    /* Verify with the library function */
    vec2 qr_lib = vec2_bezier_quadratic(qp0, qp1, qp2, t_demo);
    SDL_Log("Library:   vec2_bezier_quadratic(p0, p1, p2, 0.5)");
    print_vec2("  result", qr_lib);
    SDL_Log(" ");

    /* Sample the full quadratic curve */
    vec2 quad_samples[SAMPLE_COUNT];
    for (int i = 0; i < SAMPLE_COUNT; i++) {
        float t = (float)i / (float)(SAMPLE_COUNT - 1);
        quad_samples[i] = vec2_bezier_quadratic(qp0, qp1, qp2, t);
    }
    print_curve_samples("Quadratic Bezier curve", quad_samples, SAMPLE_COUNT);
    SDL_Log(" ");

    /* ================================================================== */
    /*  3. Cubic Bezier — De Casteljau with 4 control points              */
    /* ================================================================== */
    SDL_Log("--- 3. Cubic Bezier Curve (4 Control Points) ---");
    SDL_Log("Three rounds of lerp for four control points.");
    SDL_Log(" ");

    vec2 cp0 = vec2_create(0.0f, 0.0f);  /* Start       */
    vec2 cp1 = vec2_create(1.0f, 3.0f);  /* Guide 1     */
    vec2 cp2 = vec2_create(3.0f, 3.0f);  /* Guide 2     */
    vec2 cp3 = vec2_create(4.0f, 0.0f);  /* End         */

    SDL_Log("Control points:");
    print_vec2("  p0 (start)  ", cp0);
    print_vec2("  p1 (guide 1)", cp1);
    print_vec2("  p2 (guide 2)", cp2);
    print_vec2("  p3 (end)    ", cp3);
    SDL_Log(" ");

    /* Show De Casteljau step by step for t = 0.5 */
    SDL_Log("De Casteljau at t = %.1f:", t_demo);

    vec2 cq0 = vec2_lerp(cp0, cp1, t_demo);
    vec2 cq1 = vec2_lerp(cp1, cp2, t_demo);
    vec2 cq2 = vec2_lerp(cp2, cp3, t_demo);
    SDL_Log("  Round 1: q0 = lerp(p0, p1, 0.5) = (%.4f, %.4f)", cq0.x, cq0.y);
    SDL_Log("  Round 1: q1 = lerp(p1, p2, 0.5) = (%.4f, %.4f)", cq1.x, cq1.y);
    SDL_Log("  Round 1: q2 = lerp(p2, p3, 0.5) = (%.4f, %.4f)", cq2.x, cq2.y);

    vec2 cr0 = vec2_lerp(cq0, cq1, t_demo);
    vec2 cr1 = vec2_lerp(cq1, cq2, t_demo);
    SDL_Log("  Round 2: r0 = lerp(q0, q1, 0.5) = (%.4f, %.4f)", cr0.x, cr0.y);
    SDL_Log("  Round 2: r1 = lerp(q1, q2, 0.5) = (%.4f, %.4f)", cr1.x, cr1.y);

    vec2 cr = vec2_lerp(cr0, cr1, t_demo);
    SDL_Log("  Round 3: result = lerp(r0, r1, 0.5) = (%.4f, %.4f)", cr.x, cr.y);
    SDL_Log(" ");

    /* Verify with library */
    vec2 cr_lib = vec2_bezier_cubic(cp0, cp1, cp2, cp3, t_demo);
    SDL_Log("Library:   vec2_bezier_cubic(p0, p1, p2, p3, 0.5)");
    print_vec2("  result", cr_lib);
    SDL_Log(" ");

    /* Sample the full cubic curve */
    vec2 cubic_samples[SAMPLE_COUNT];
    for (int i = 0; i < SAMPLE_COUNT; i++) {
        float t = (float)i / (float)(SAMPLE_COUNT - 1);
        cubic_samples[i] = vec2_bezier_cubic(cp0, cp1, cp2, cp3, t);
    }
    print_curve_samples("Cubic Bezier curve", cubic_samples, SAMPLE_COUNT);
    SDL_Log(" ");

    /* ================================================================== */
    /*  4. Tangent vectors — direction and speed along the curve           */
    /* ================================================================== */
    SDL_Log("--- 4. Tangent Vectors ---");
    SDL_Log("The tangent is the first derivative dB/dt.");
    SDL_Log("It tells you the direction of travel along the curve.");
    SDL_Log(" ");

    /* Quadratic tangent */
    SDL_Log("Quadratic Bezier tangent:");
    for (int i = 0; i <= DEMO_STEPS; i++) {
        float t = (float)i / (float)DEMO_STEPS;
        vec2 tan = vec2_bezier_quadratic_tangent(qp0, qp1, qp2, t);
        float mag = vec2_length(tan);
        SDL_Log("  t=%.2f  tangent=(%.4f, %.4f)  |tangent|=%.4f",
                t, tan.x, tan.y, mag);
    }
    SDL_Log(" ");

    /* Cubic tangent */
    SDL_Log("Cubic Bezier tangent:");
    for (int i = 0; i <= DEMO_STEPS; i++) {
        float t = (float)i / (float)DEMO_STEPS;
        vec2 tan = vec2_bezier_cubic_tangent(cp0, cp1, cp2, cp3, t);
        float mag = vec2_length(tan);
        SDL_Log("  t=%.2f  tangent=(%.4f, %.4f)  |tangent|=%.4f",
                t, tan.x, tan.y, mag);
    }
    SDL_Log(" ");

    SDL_Log("At t=0, the tangent points from p0 toward p1.");
    SDL_Log("At t=1, the tangent points from p(n-1) toward pn.");
    SDL_Log("This is why control points determine departure/arrival direction.");
    SDL_Log(" ");

    /* ================================================================== */
    /*  5. Bernstein basis polynomials — the weighting functions           */
    /* ================================================================== */
    SDL_Log("--- 5. Bernstein Basis Polynomials ---");
    SDL_Log("Each control point's influence is weighted by a Bernstein polynomial.");
    SDL_Log("The weights are always non-negative and sum to 1 (partition of unity).");
    SDL_Log(" ");

    /* Quadratic Bernstein basis: B(0,2)=(1-t)^2, B(1,2)=2(1-t)t, B(2,2)=t^2 */
    SDL_Log("Quadratic basis (n=2):");
    SDL_Log("  t     B(0,2)    B(1,2)    B(2,2)    sum");
    for (int i = 0; i <= DEMO_STEPS; i++) {
        float t = (float)i / (float)DEMO_STEPS;
        float u = 1.0f - t;
        float b0 = u * u;
        float b1 = 2.0f * u * t;
        float b2 = t * t;
        SDL_Log("  %.2f   %.4f    %.4f    %.4f    %.4f",
                t, b0, b1, b2, b0 + b1 + b2);
    }
    SDL_Log(" ");

    /* Cubic Bernstein basis */
    SDL_Log("Cubic basis (n=3):");
    SDL_Log("  t     B(0,3)    B(1,3)    B(2,3)    B(3,3)    sum");
    for (int i = 0; i <= DEMO_STEPS; i++) {
        float t = (float)i / (float)DEMO_STEPS;
        float u = 1.0f - t;
        float b0 = u * u * u;
        float b1 = 3.0f * u * u * t;
        float b2 = 3.0f * u * t * t;
        float b3 = t * t * t;
        SDL_Log("  %.2f   %.4f    %.4f    %.4f    %.4f    %.4f",
                t, b0, b1, b2, b3, b0 + b1 + b2 + b3);
    }
    SDL_Log("Every row sums to 1.0 -- the curve point is a weighted average.");
    SDL_Log(" ");

    /* ================================================================== */
    /*  6. Control-point influence — how guide points shape the curve      */
    /* ================================================================== */
    SDL_Log("--- 6. Control-Point Influence ---");
    SDL_Log("Moving a guide point changes the curve shape.");
    SDL_Log(" ");

    /* Same endpoints, different guide point heights */
    vec2 flat_guide  = vec2_create(2.0f, 1.0f);
    vec2 high_guide  = vec2_create(2.0f, 6.0f);

    vec2 mid_flat = vec2_bezier_quadratic(qp0, flat_guide, qp2, 0.5f);
    vec2 mid_high = vec2_bezier_quadratic(qp0, high_guide, qp2, 0.5f);

    SDL_Log("Same start (0,0) and end (4,0) with different guides:");
    SDL_Log("  Guide at (2, 1): midpoint = (%.4f, %.4f)", mid_flat.x, mid_flat.y);
    SDL_Log("  Guide at (2, 6): midpoint = (%.4f, %.4f)", mid_high.x, mid_high.y);
    SDL_Log("Higher guide = stronger pull = more pronounced curve.");
    SDL_Log(" ");

    /* ================================================================== */
    /*  7. Endpoint interpolation — the curve always passes through       */
    /*     its first and last control points                              */
    /* ================================================================== */
    SDL_Log("--- 7. Endpoint Interpolation Property ---");
    SDL_Log("Bezier curves ALWAYS pass through the first and last control points.");
    SDL_Log(" ");

    vec2 start = vec2_bezier_cubic(cp0, cp1, cp2, cp3, 0.0f);
    vec2 end   = vec2_bezier_cubic(cp0, cp1, cp2, cp3, 1.0f);

    SDL_Log("Cubic Bezier at t=0: (%.4f, %.4f) = p0 = (%.4f, %.4f)",
            start.x, start.y, cp0.x, cp0.y);
    SDL_Log("Cubic Bezier at t=1: (%.4f, %.4f) = p3 = (%.4f, %.4f)",
            end.x, end.y, cp3.x, cp3.y);
    SDL_Log(" ");

    /* ================================================================== */
    /*  8. Convex hull property — curve stays inside control points        */
    /* ================================================================== */
    SDL_Log("--- 8. Convex Hull Property ---");
    SDL_Log("A Bezier curve always lies inside the bounding box of its");
    SDL_Log("control points (and more specifically, their convex hull).");
    SDL_Log(" ");

    /* Compute axis-aligned bounding box of cubic control points */
    float bb_min_x = cp0.x, bb_max_x = cp0.x;
    float bb_min_y = cp0.y, bb_max_y = cp0.y;
    vec2 cpts[] = {cp1, cp2, cp3};
    for (int i = 0; i < 3; i++) {
        if (cpts[i].x < bb_min_x) bb_min_x = cpts[i].x;
        if (cpts[i].x > bb_max_x) bb_max_x = cpts[i].x;
        if (cpts[i].y < bb_min_y) bb_min_y = cpts[i].y;
        if (cpts[i].y > bb_max_y) bb_max_y = cpts[i].y;
    }
    SDL_Log("Control point bounding box: x=[%.1f, %.1f]  y=[%.1f, %.1f]",
            bb_min_x, bb_max_x, bb_min_y, bb_max_y);

    /* Check that all sampled curve points lie within the bounding box */
    int all_inside = 1;
    for (int i = 0; i < SAMPLE_COUNT; i++) {
        if (cubic_samples[i].x < bb_min_x - FLOAT_TOLERANCE ||
            cubic_samples[i].x > bb_max_x + FLOAT_TOLERANCE ||
            cubic_samples[i].y < bb_min_y - FLOAT_TOLERANCE ||
            cubic_samples[i].y > bb_max_y + FLOAT_TOLERANCE) {
            all_inside = 0;
            break;
        }
    }
    SDL_Log("All %d curve samples inside bounding box: %s",
            SAMPLE_COUNT, all_inside ? "yes" : "no");
    SDL_Log("This follows from Bernstein weights being non-negative and summing to 1.");
    SDL_Log(" ");

    /* ================================================================== */
    /*  9. Arc length — approximating how long the curve is               */
    /* ================================================================== */
    SDL_Log("--- 9. Arc-Length Approximation ---");
    SDL_Log("Bezier curves have no simple formula for arc length.");
    SDL_Log("We approximate by summing short straight segments.");
    SDL_Log(" ");

    /* Compare accuracy at different segment counts */
    int segment_counts[] = {4, 8, 16, 32, 64, 128};
    int num_tests = 6;

    SDL_Log("Cubic Bezier arc length with increasing segments:");
    for (int i = 0; i < num_tests; i++) {
        float len = vec2_bezier_cubic_length(cp0, cp1, cp2, cp3,
                                             segment_counts[i]);
        SDL_Log("  %3d segments -> length = %.6f", segment_counts[i], len);
    }
    SDL_Log("The value converges as segments increase.");
    SDL_Log(" ");

    /* Compare: straight-line distance vs arc length */
    float straight = vec2_length(vec2_sub(cp3, cp0));
    float arc = vec2_bezier_cubic_length(cp0, cp1, cp2, cp3, 128);
    SDL_Log("Straight-line distance p0->p3: %.4f", straight);
    SDL_Log("Curve arc length (128 segs):   %.4f", arc);
    SDL_Log("The curve is always at least as long as the straight line.");
    SDL_Log(" ");

    /* ================================================================== */
    /*  10. Joining curves — C0 and C1 continuity                         */
    /* ================================================================== */
    SDL_Log("--- 10. Joining Bezier Curves (Continuity) ---");
    SDL_Log("Multiple Bezier curves can be chained into a longer path.");
    SDL_Log(" ");

    /* Two cubic segments sharing an endpoint */
    vec2 s1_p0 = vec2_create(0.0f, 0.0f);
    vec2 s1_p1 = vec2_create(1.0f, 2.0f);
    vec2 s1_p2 = vec2_create(2.0f, 2.0f);
    vec2 s1_p3 = vec2_create(3.0f, 0.0f);

    /* Second segment starts where the first ends (C0 continuity) */
    vec2 s2_p0 = s1_p3;                    /* Shared endpoint */
    vec2 s2_p1 = vec2_create(4.0f, -2.0f);
    vec2 s2_p2 = vec2_create(5.0f, -2.0f);
    vec2 s2_p3 = vec2_create(6.0f, 0.0f);

    /* C0 continuity: endpoints match */
    vec2 end1   = vec2_bezier_cubic(s1_p0, s1_p1, s1_p2, s1_p3, 1.0f);
    vec2 start2 = vec2_bezier_cubic(s2_p0, s2_p1, s2_p2, s2_p3, 0.0f);

    SDL_Log("C0 continuity (shared endpoint):");
    SDL_Log("  Segment 1 at t=1: (%.4f, %.4f)", end1.x, end1.y);
    SDL_Log("  Segment 2 at t=0: (%.4f, %.4f)", start2.x, start2.y);
    SDL_Log("  Match: %s", (fabsf(end1.x - start2.x) < FLOAT_TOLERANCE &&
                             fabsf(end1.y - start2.y) < FLOAT_TOLERANCE)
             ? "yes" : "no");
    SDL_Log(" ");

    /* C1 continuity: tangent direction and magnitude also match.
     * For this, s2_p1 must be a reflection of s1_p2 across the junction. */
    vec2 tan1_end   = vec2_bezier_cubic_tangent(s1_p0, s1_p1, s1_p2, s1_p3, 1.0f);
    vec2 tan2_start = vec2_bezier_cubic_tangent(s2_p0, s2_p1, s2_p2, s2_p3, 0.0f);

    SDL_Log("Tangent at junction:");
    SDL_Log("  Segment 1 at t=1: (%.4f, %.4f)", tan1_end.x, tan1_end.y);
    SDL_Log("  Segment 2 at t=0: (%.4f, %.4f)", tan2_start.x, tan2_start.y);
    SDL_Log(" ");

    /* Now make C1: place s2_p1 so the tangent matches */
    SDL_Log("For C1 continuity, the first guide of segment 2 must be placed");
    SDL_Log("so that the tangent direction and speed match at the junction.");
    SDL_Log("Rule: s2_p1 = s1_p3 + (s1_p3 - s1_p2)");

    vec2 s2_p1_c1 = vec2_add(s1_p3, vec2_sub(s1_p3, s1_p2));
    SDL_Log("  s1_p2 = (%.1f, %.1f),  s1_p3 = (%.1f, %.1f)",
            s1_p2.x, s1_p2.y, s1_p3.x, s1_p3.y);
    SDL_Log("  C1 guide: s2_p1 = (%.1f, %.1f)", s2_p1_c1.x, s2_p1_c1.y);

    vec2 tan1_c1 = vec2_bezier_cubic_tangent(s1_p0, s1_p1, s1_p2, s1_p3, 1.0f);
    vec2 tan2_c1 = vec2_bezier_cubic_tangent(s2_p0, s2_p1_c1, s2_p2, s2_p3, 0.0f);
    SDL_Log("  Tangent seg 1 end:   (%.4f, %.4f)", tan1_c1.x, tan1_c1.y);
    SDL_Log("  Tangent seg 2 start: (%.4f, %.4f)", tan2_c1.x, tan2_c1.y);
    SDL_Log("  Match: %s", (fabsf(tan1_c1.x - tan2_c1.x) < FLOAT_TOLERANCE &&
                             fabsf(tan1_c1.y - tan2_c1.y) < FLOAT_TOLERANCE)
             ? "yes" : "no");
    SDL_Log(" ");

    /* ================================================================== */
    /*  11. Curve splitting — De Casteljau subdivision                     */
    /* ================================================================== */
    SDL_Log("--- 11. Curve Splitting (Subdivision) ---");
    SDL_Log("De Casteljau's algorithm naturally splits a curve into two halves.");
    SDL_Log("Each half is itself a valid Bezier curve.");
    SDL_Log(" ");

    vec2 left[4], right[4];
    vec2_bezier_cubic_split(cp0, cp1, cp2, cp3, 0.5f, left, right);

    SDL_Log("Splitting cubic curve at t=0.5:");
    SDL_Log("  Left half:  (%5.2f,%5.2f) (%5.2f,%5.2f) (%5.2f,%5.2f) (%5.2f,%5.2f)",
            left[0].x, left[0].y, left[1].x, left[1].y,
            left[2].x, left[2].y, left[3].x, left[3].y);
    SDL_Log("  Right half: (%5.2f,%5.2f) (%5.2f,%5.2f) (%5.2f,%5.2f) (%5.2f,%5.2f)",
            right[0].x, right[0].y, right[1].x, right[1].y,
            right[2].x, right[2].y, right[3].x, right[3].y);
    SDL_Log(" ");

    /* Verify the split produces the same curve */
    vec2 orig_pt = vec2_bezier_cubic(cp0, cp1, cp2, cp3, 0.25f);
    vec2 left_pt = vec2_bezier_cubic(left[0], left[1], left[2], left[3], 0.5f);
    SDL_Log("Verification: original at t=0.25 vs left half at t=0.5:");
    SDL_Log("  Original: (%.4f, %.4f)", orig_pt.x, orig_pt.y);
    SDL_Log("  Left:     (%.4f, %.4f)  (match: %s)",
            left_pt.x, left_pt.y,
            (fabsf(orig_pt.x - left_pt.x) < FLOAT_TOLERANCE &&
             fabsf(orig_pt.y - left_pt.y) < FLOAT_TOLERANCE) ? "yes" : "no");
    SDL_Log(" ");

    /* ================================================================== */
    /*  12. Degree elevation — quadratic to cubic                         */
    /* ================================================================== */
    SDL_Log("--- 12. Degree Elevation (Quadratic -> Cubic) ---");
    SDL_Log("Every quadratic Bezier can be exactly represented as a cubic.");
    SDL_Log("TrueType fonts use quadratic curves; this converts them to cubic.");
    SDL_Log(" ");

    vec2 cubic_equiv[4];
    vec2_bezier_quadratic_to_cubic(qp0, qp1, qp2, cubic_equiv);

    SDL_Log("Quadratic: p0=(%.1f,%.1f) p1=(%.1f,%.1f) p2=(%.1f,%.1f)",
            qp0.x, qp0.y, qp1.x, qp1.y, qp2.x, qp2.y);
    SDL_Log("Cubic:     p0=(%.4f,%.4f) p1=(%.4f,%.4f) p2=(%.4f,%.4f) p3=(%.4f,%.4f)",
            cubic_equiv[0].x, cubic_equiv[0].y,
            cubic_equiv[1].x, cubic_equiv[1].y,
            cubic_equiv[2].x, cubic_equiv[2].y,
            cubic_equiv[3].x, cubic_equiv[3].y);
    SDL_Log(" ");

    /* Verify they trace the same path */
    SDL_Log("Verification at 5 t values:");
    float verify_ts[] = {0.0f, 0.25f, 0.5f, 0.75f, 1.0f};
    for (int i = 0; i < 5; i++) {
        float tv = verify_ts[i];
        vec2 pq = vec2_bezier_quadratic(qp0, qp1, qp2, tv);
        vec2 pc = vec2_bezier_cubic(cubic_equiv[0], cubic_equiv[1],
                                    cubic_equiv[2], cubic_equiv[3], tv);
        SDL_Log("  t=%.2f  quad=(%.4f,%.4f)  cubic=(%.4f,%.4f)  match: %s",
                tv, pq.x, pq.y, pc.x, pc.y,
                (fabsf(pq.x - pc.x) < FLOAT_TOLERANCE &&
                 fabsf(pq.y - pc.y) < FLOAT_TOLERANCE) ? "yes" : "no");
    }
    SDL_Log(" ");

    /* ================================================================== */
    /*  13. Adaptive flattening — curves to line segments                  */
    /* ================================================================== */
    SDL_Log("--- 13. Adaptive Flattening ---");
    SDL_Log("Recursively subdivide until each piece is flat enough,");
    SDL_Log("then approximate with line segments. Core of font rendering.");
    SDL_Log(" ");

    /* Flatten a cubic curve at different tolerances */
    float tolerances[] = {2.0f, 0.5f, 0.1f, 0.01f};
    int num_tol = 4;

    SDL_Log("Flattening cubic Bezier at different tolerances:");
    for (int i = 0; i < num_tol; i++) {
        vec2 flat_pts[FLATTEN_MAX_POINTS];
        int flat_count = 0;
        flat_pts[flat_count++] = cp0;
        vec2_bezier_cubic_flatten(cp0, cp1, cp2, cp3, tolerances[i],
                                  flat_pts, FLATTEN_MAX_POINTS, &flat_count);
        SDL_Log("  tolerance=%.2f -> %d line segments (%d points)",
                tolerances[i], flat_count - 1, flat_count);
    }
    SDL_Log("Tighter tolerance = more segments = closer to the true curve.");
    SDL_Log(" ");

    /* ================================================================== */
    /*  14. Summary                                                       */
    /* ================================================================== */
    SDL_Log("--- Summary ---");
    SDL_Log("Bezier curves are built entirely from linear interpolation (lerp).");
    SDL_Log("  Quadratic: 3 control points, 2 rounds of lerp");
    SDL_Log("  Cubic:     4 control points, 3 rounds of lerp");
    SDL_Log("Key properties:");
    SDL_Log("  - Always pass through first and last control points");
    SDL_Log("  - Tangent at endpoints determined by adjacent control points");
    SDL_Log("  - Lie entirely within the convex hull of control points");
    SDL_Log("  - Can be chained with C0 or C1 continuity for complex paths");
    SDL_Log(" ");

    SDL_Quit();
    return 0;
}
