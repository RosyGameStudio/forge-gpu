#!/usr/bin/env python
"""
forge_diagrams.py — Generate matplotlib diagrams for forge-gpu lesson READMEs.

Produces PNG diagrams at 200 DPI for embedding in markdown. Each lesson's
diagrams are placed in its assets/ directory (created automatically).

Usage:
    python scripts/forge_diagrams.py --lesson math/01    # one lesson
    python scripts/forge_diagrams.py --all               # all diagrams
    python scripts/forge_diagrams.py --list               # list available

Requires: pip install numpy matplotlib
"""

import argparse
import os
import sys

import matplotlib.pyplot as plt
import numpy as np
from matplotlib.patches import FancyArrowPatch, Polygon

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
LESSONS_DIR = os.path.join(REPO_ROOT, "lessons")
DPI = 200


# ---------------------------------------------------------------------------
# Shared style
# ---------------------------------------------------------------------------

COLORS = {
    "vec_a": "#2196F3",  # blue
    "vec_b": "#F44336",  # red
    "vec_sum": "#4CAF50",  # green
    "vec_neg": "#9C27B0",  # purple
    "grid": "#BDBDBD",  # light grey
    "highlight": "#FF9800",  # orange
    "text": "#212121",  # near-black
    "bg": "#FFFFFF",  # white
    "axis": "#757575",  # grey
    "lerp_a": "#1565C0",  # dark blue
    "lerp_b": "#C62828",  # dark red
    "lerp_result": "#2E7D32",  # dark green
}


def _style_axis(ax, xlim=None, ylim=None, grid=True):
    """Apply consistent styling to an axis."""
    ax.set_facecolor(COLORS["bg"])
    ax.set_aspect("equal")
    if xlim:
        ax.set_xlim(xlim)
    if ylim:
        ax.set_ylim(ylim)
    if grid:
        ax.grid(True, alpha=0.3, color=COLORS["grid"], linewidth=0.5)
    ax.tick_params(labelsize=7)


def _draw_vector(ax, origin, vec, color, label, label_offset=(0.1, 0.1), lw=2.0):
    """Draw a 2D vector arrow with a label."""
    arrow = FancyArrowPatch(
        origin,
        (origin[0] + vec[0], origin[1] + vec[1]),
        arrowstyle="->,head_length=6,head_width=4",
        color=color,
        linewidth=lw,
        mutation_scale=1,
    )
    ax.add_patch(arrow)
    mid_x = origin[0] + vec[0] / 2 + label_offset[0]
    mid_y = origin[1] + vec[1] / 2 + label_offset[1]
    ax.text(
        mid_x,
        mid_y,
        label,
        color=color,
        fontsize=9,
        fontweight="bold",
        ha="center",
        va="center",
    )


def _save(fig, lesson_path, filename):
    """Save a figure to a lesson's assets/ directory."""
    assets_dir = os.path.join(LESSONS_DIR, lesson_path, "assets")
    os.makedirs(assets_dir, exist_ok=True)
    out = os.path.join(assets_dir, filename)
    fig.savefig(out, dpi=DPI, bbox_inches="tight", facecolor=COLORS["bg"])
    plt.close(fig)
    rel = os.path.relpath(out, REPO_ROOT)
    print(f"  {rel}")


# ---------------------------------------------------------------------------
# math/01-vectors — vector_addition.png
# ---------------------------------------------------------------------------


def diagram_vector_addition():
    """Vector addition with tail-to-head and parallelogram."""
    fig, ax = plt.subplots(figsize=(5, 4))
    _style_axis(ax, xlim=(-0.5, 5), ylim=(-0.5, 4.5))
    ax.set_title("Vector Addition: a + b", fontsize=11, fontweight="bold", pad=10)

    a = (3, 1)
    b = (1, 2)

    # Parallelogram fill
    para = Polygon(
        [(0, 0), a, (a[0] + b[0], a[1] + b[1]), b],
        closed=True,
        alpha=0.08,
        facecolor=COLORS["vec_sum"],
        edgecolor="none",
    )
    ax.add_patch(para)

    # Vectors
    _draw_vector(ax, (0, 0), a, COLORS["vec_a"], "a", label_offset=(0.0, -0.3))
    _draw_vector(ax, a, b, COLORS["vec_b"], "b", label_offset=(0.25, 0.0))
    _draw_vector(
        ax,
        (0, 0),
        (a[0] + b[0], a[1] + b[1]),
        COLORS["vec_sum"],
        "a + b",
        label_offset=(-0.5, 0.2),
    )

    # Dashed parallelogram sides
    ax.plot(
        [b[0], a[0] + b[0]],
        [b[1], a[1] + b[1]],
        "--",
        color=COLORS["vec_a"],
        alpha=0.4,
        lw=1,
    )
    ax.plot(
        [0, b[0]],
        [0, b[1]],
        "--",
        color=COLORS["vec_b"],
        alpha=0.4,
        lw=1,
    )

    # Origin dot
    ax.plot(0, 0, "ko", markersize=4)

    _save(fig, "math/01-vectors", "vector_addition.png")


# ---------------------------------------------------------------------------
# math/01-vectors — dot_product.png
# ---------------------------------------------------------------------------


def diagram_dot_product():
    """Three-panel dot product: same, perpendicular, opposite."""
    fig, axes = plt.subplots(1, 3, figsize=(9, 3))
    cases = [
        ("Same direction", (1, 0.3), (0.8, 0.24), "> 0"),
        ("Perpendicular", (1, 0), (0, 1), "= 0"),
        ("Opposite", (1, 0.3), (-0.8, -0.24), "< 0"),
    ]

    for ax, (title, a, b, sign) in zip(axes, cases):
        _style_axis(ax, xlim=(-1.5, 1.5), ylim=(-1.0, 1.5))
        ax.set_title(f"{title}\na · b {sign}", fontsize=9, fontweight="bold")

        _draw_vector(ax, (0, 0), a, COLORS["vec_a"], "a", label_offset=(0.0, -0.2))
        _draw_vector(ax, (0, 0), b, COLORS["vec_b"], "b", label_offset=(0.0, 0.2))
        ax.plot(0, 0, "ko", markersize=3)

    fig.suptitle("Dot Product", fontsize=12, fontweight="bold", y=1.05)
    fig.tight_layout()
    _save(fig, "math/01-vectors", "dot_product.png")


# ---------------------------------------------------------------------------
# math/03-bilinear-interpolation — bilinear_interpolation.png
# ---------------------------------------------------------------------------


def diagram_bilinear_interpolation():
    """Grid cell with the 3 lerp steps highlighted."""
    fig, ax = plt.subplots(figsize=(5, 5))
    _style_axis(ax, xlim=(-0.3, 1.5), ylim=(-0.3, 1.5), grid=False)
    ax.set_title("Bilinear Interpolation\nThree lerps", fontsize=11, fontweight="bold")

    # Grid cell
    corners = {"c00": (0, 0), "c10": (1, 0), "c01": (0, 1), "c11": (1, 1)}
    cell = Polygon(
        [corners["c00"], corners["c10"], corners["c11"], corners["c01"]],
        closed=True,
        fill=False,
        edgecolor=COLORS["axis"],
        linewidth=1.5,
    )
    ax.add_patch(cell)

    # Corner labels
    for name, (cx, cy) in corners.items():
        ax.plot(cx, cy, "o", color=COLORS["text"], markersize=6)
        offset_x = -0.15 if cx == 0 else 0.1
        offset_y = -0.12 if cy == 0 else 0.1
        ax.text(
            cx + offset_x,
            cy + offset_y,
            name,
            fontsize=8,
            fontweight="bold",
            color=COLORS["text"],
            ha="center",
        )

    tx, ty = 0.35, 0.7

    # Step 1: bottom lerp
    bot = (tx, 0)
    ax.plot(*bot, "s", color=COLORS["lerp_a"], markersize=8, zorder=5)
    ax.annotate(
        "1. bot = lerp(c00, c10, tx)",
        bot,
        xytext=(0.6, -0.2),
        fontsize=7,
        color=COLORS["lerp_a"],
        fontweight="bold",
        arrowprops={"arrowstyle": "->", "color": COLORS["lerp_a"], "lw": 1},
    )

    # Step 2: top lerp
    top = (tx, 1)
    ax.plot(*top, "s", color=COLORS["lerp_b"], markersize=8, zorder=5)
    ax.annotate(
        "2. top = lerp(c01, c11, tx)",
        top,
        xytext=(0.6, 1.2),
        fontsize=7,
        color=COLORS["lerp_b"],
        fontweight="bold",
        arrowprops={"arrowstyle": "->", "color": COLORS["lerp_b"], "lw": 1},
    )

    # Step 3: vertical lerp
    result = (tx, ty)
    ax.plot([tx, tx], [0, 1], "--", color=COLORS["lerp_result"], alpha=0.5, lw=1)
    ax.plot(*result, "*", color=COLORS["lerp_result"], markersize=14, zorder=6)
    ax.annotate(
        "3. result = lerp(bot, top, ty)",
        result,
        xytext=(1.0, 0.55),
        fontsize=7,
        color=COLORS["lerp_result"],
        fontweight="bold",
        arrowprops={"arrowstyle": "->", "color": COLORS["lerp_result"], "lw": 1},
    )

    # tx/ty labels
    ax.annotate(
        f"tx = {tx}",
        (tx, -0.05),
        fontsize=7,
        ha="center",
        color=COLORS["axis"],
    )
    ax.annotate(
        f"ty = {ty}",
        (-0.05, ty),
        fontsize=7,
        ha="right",
        va="center",
        color=COLORS["axis"],
    )

    _save(fig, "math/03-bilinear-interpolation", "bilinear_interpolation.png")


# ---------------------------------------------------------------------------
# math/05-matrices — matrix_basis_vectors.png
# ---------------------------------------------------------------------------


def diagram_matrix_basis_vectors():
    """Before/after basis vectors for a 45-degree rotation."""
    fig, axes = plt.subplots(1, 2, figsize=(8, 4))

    angle = np.pi / 4  # 45 degrees
    cos_a, sin_a = np.cos(angle), np.sin(angle)

    for idx, (ax, title, x_vec, y_vec) in enumerate(
        zip(
            axes,
            ["Before (Identity)", "After (45° rotation)"],
            [(1, 0), (cos_a, sin_a)],
            [(0, 1), (-sin_a, cos_a)],
        )
    ):
        _style_axis(ax, xlim=(-1.5, 1.5), ylim=(-0.5, 1.5))
        ax.set_title(title, fontsize=10, fontweight="bold")

        # Draw basis vectors
        x_label_offset = (0.15, -0.15) if idx == 0 else (0.2, -0.05)
        y_label_offset = (-0.25, 0.0) if idx == 0 else (-0.15, 0.15)

        _draw_vector(
            ax, (0, 0), x_vec, COLORS["vec_a"], "X", label_offset=x_label_offset
        )
        _draw_vector(
            ax, (0, 0), y_vec, COLORS["vec_b"], "Y", label_offset=y_label_offset
        )
        ax.plot(0, 0, "ko", markersize=4)

        # Unit circle arc for reference
        theta = np.linspace(0, 2 * np.pi, 100)
        ax.plot(
            np.cos(theta), np.sin(theta), "-", color=COLORS["grid"], alpha=0.3, lw=0.5
        )

    fig.suptitle(
        "Matrix Basis Vectors (columns)", fontsize=12, fontweight="bold", y=1.02
    )
    fig.tight_layout()
    _save(fig, "math/05-matrices", "matrix_basis_vectors.png")


# ---------------------------------------------------------------------------
# math/06-projections — frustum.png
# ---------------------------------------------------------------------------


def diagram_frustum():
    """Viewing frustum side view with near/far planes and FOV."""
    fig, ax = plt.subplots(figsize=(7, 4))
    ax.set_facecolor(COLORS["bg"])
    ax.set_aspect("equal")
    ax.grid(False)
    ax.set_title("Viewing Frustum (side view)", fontsize=11, fontweight="bold")

    eye = (0, 0)
    near = 1.5
    far = 6
    fov_half = np.radians(30)

    near_h = near * np.tan(fov_half)
    far_h = far * np.tan(fov_half)

    # Frustum trapezoid
    frustum = Polygon(
        [
            (near, -near_h),
            (far, -far_h),
            (far, far_h),
            (near, near_h),
        ],
        closed=True,
        alpha=0.1,
        facecolor=COLORS["vec_a"],
        edgecolor=COLORS["vec_a"],
        linewidth=1.5,
    )
    ax.add_patch(frustum)

    # Eye point and rays
    ax.plot(*eye, "ko", markersize=6, zorder=5)
    ax.text(-0.3, 0.0, "Eye", fontsize=9, fontweight="bold", ha="right", va="center")

    ax.plot([0, far], [0, far_h], "-", color=COLORS["axis"], alpha=0.4, lw=1)
    ax.plot([0, far], [0, -far_h], "-", color=COLORS["axis"], alpha=0.4, lw=1)

    # Near plane
    ax.plot([near, near], [-near_h, near_h], "-", color=COLORS["vec_sum"], lw=2)
    ax.text(
        near,
        near_h + 0.25,
        "Near plane",
        fontsize=8,
        ha="center",
        color=COLORS["vec_sum"],
        fontweight="bold",
    )

    # Far plane
    ax.plot([far, far], [-far_h, far_h], "-", color=COLORS["vec_b"], lw=2)
    ax.text(
        far,
        far_h + 0.25,
        "Far plane",
        fontsize=8,
        ha="center",
        color=COLORS["vec_b"],
        fontweight="bold",
    )

    # FOV angle arc
    theta = np.linspace(-fov_half, fov_half, 30)
    arc_r = 1.0
    ax.plot(
        arc_r * np.cos(theta),
        arc_r * np.sin(theta),
        "-",
        color=COLORS["highlight"],
        lw=1.5,
    )
    ax.text(
        1.1,
        0.0,
        "FOV",
        fontsize=8,
        color=COLORS["highlight"],
        fontweight="bold",
        ha="left",
        va="center",
    )

    # Depth axis
    ax.annotate(
        "",
        xy=(far + 0.3, 0),
        xytext=(-0.5, 0),
        arrowprops={"arrowstyle": "->", "color": COLORS["axis"], "lw": 0.8},
    )
    ax.text(
        far + 0.5,
        0,
        "-Z (into screen)",
        fontsize=7,
        color=COLORS["axis"],
        va="center",
    )

    ax.set_xlim(-1, far + 2.5)
    ax.set_ylim(-far_h - 1, far_h + 1)
    ax.set_xticks([])
    ax.set_yticks([])
    for spine in ax.spines.values():
        spine.set_visible(False)

    _save(fig, "math/06-projections", "frustum.png")


# ---------------------------------------------------------------------------
# gpu/04-textures-and-samplers — uv_mapping.png
# ---------------------------------------------------------------------------


def diagram_uv_mapping():
    """Position space to UV space mapping."""
    fig, axes = plt.subplots(1, 2, figsize=(8, 4))

    # Position space
    ax = axes[0]
    _style_axis(ax, xlim=(-1.0, 1.0), ylim=(-1.0, 1.0))
    ax.set_title("Position Space", fontsize=10, fontweight="bold")

    quad = Polygon(
        [(-0.6, -0.6), (0.6, -0.6), (0.6, 0.6), (-0.6, 0.6)],
        closed=True,
        alpha=0.15,
        facecolor=COLORS["vec_a"],
        edgecolor=COLORS["vec_a"],
        linewidth=1.5,
    )
    ax.add_patch(quad)

    pos_verts = [(-0.6, -0.6), (0.6, -0.6), (0.6, 0.6), (-0.6, 0.6)]
    pos_labels = ["v3", "v2", "v1", "v0"]
    for (px, py), label in zip(pos_verts, pos_labels):
        ax.plot(px, py, "o", color=COLORS["vec_a"], markersize=6, zorder=5)
        ox = -0.12 if px < 0 else 0.08
        oy = -0.1 if py < 0 else 0.08
        ax.text(
            px + ox,
            py + oy,
            label,
            fontsize=8,
            fontweight="bold",
            color=COLORS["vec_a"],
        )

    ax.set_xlabel("X", fontsize=8)
    ax.set_ylabel("Y", fontsize=8)

    # UV space
    ax = axes[1]
    _style_axis(ax, xlim=(-0.2, 1.3), ylim=(-0.2, 1.3))
    ax.set_title("UV Space (Texture)", fontsize=10, fontweight="bold")

    uv_quad = Polygon(
        [(0, 0), (1, 0), (1, 1), (0, 1)],
        closed=True,
        alpha=0.15,
        facecolor=COLORS["highlight"],
        edgecolor=COLORS["highlight"],
        linewidth=1.5,
    )
    ax.add_patch(uv_quad)

    uv_verts = [(0, 1), (1, 1), (1, 0), (0, 0)]
    uv_labels = ["(0,1)", "(1,1)", "(1,0)", "(0,0)"]
    for (ux, uy), label in zip(uv_verts, uv_labels):
        ax.plot(ux, uy, "o", color=COLORS["highlight"], markersize=6, zorder=5)
        ox = -0.15 if ux == 0 else 0.05
        oy = -0.1 if uy == 1 else 0.08
        ax.text(
            ux + ox,
            uy + oy,
            label,
            fontsize=7,
            fontweight="bold",
            color=COLORS["highlight"],
        )

    ax.set_xlabel("U", fontsize=8)
    ax.set_ylabel("V", fontsize=8)

    # Arrow between panels
    fig.text(
        0.5,
        0.5,
        "UV\nmapping",
        ha="center",
        va="center",
        fontsize=9,
        fontweight="bold",
        color=COLORS["axis"],
        transform=fig.transFigure,
    )

    fig.tight_layout()
    _save(fig, "gpu/04-textures-and-samplers", "uv_mapping.png")


# ---------------------------------------------------------------------------
# gpu/04-textures-and-samplers — filtering_comparison.png
# ---------------------------------------------------------------------------


def diagram_filtering_comparison():
    """NEAREST vs LINEAR filtering comparison."""
    fig, axes = plt.subplots(1, 2, figsize=(8, 4))

    # Create a small 4x4 checkerboard-like pattern
    rng = np.random.default_rng(42)
    data_small = rng.integers(50, 255, size=(4, 4)).astype(float)

    # NEAREST — blocky upscale
    ax = axes[0]
    ax.set_title("NEAREST Filtering\n(no blending)", fontsize=10, fontweight="bold")
    ax.imshow(
        data_small,
        interpolation="nearest",
        cmap="gray",
        vmin=0,
        vmax=255,
        extent=[0, 4, 0, 4],
    )
    # Draw grid lines for texel boundaries
    for i in range(5):
        ax.axhline(i, color="white", lw=0.5, alpha=0.5)
        ax.axvline(i, color="white", lw=0.5, alpha=0.5)
    ax.set_xlabel("Texels", fontsize=8)
    ax.set_xticks([])
    ax.set_yticks([])

    # LINEAR — smooth upscale
    ax = axes[1]
    ax.set_title("LINEAR Filtering\n(bilinear blend)", fontsize=10, fontweight="bold")
    ax.imshow(
        data_small,
        interpolation="bilinear",
        cmap="gray",
        vmin=0,
        vmax=255,
        extent=[0, 4, 0, 4],
    )
    for i in range(5):
        ax.axhline(i, color="white", lw=0.5, alpha=0.3)
        ax.axvline(i, color="white", lw=0.5, alpha=0.3)
    ax.set_xlabel("Texels", fontsize=8)
    ax.set_xticks([])
    ax.set_yticks([])

    fig.suptitle("Texture Filtering Comparison", fontsize=12, fontweight="bold", y=1.02)
    fig.tight_layout()
    _save(fig, "gpu/04-textures-and-samplers", "filtering_comparison.png")


# ---------------------------------------------------------------------------
# Diagram registry
# ---------------------------------------------------------------------------

DIAGRAMS = {
    "math/01": [
        ("vector_addition.png", diagram_vector_addition),
        ("dot_product.png", diagram_dot_product),
    ],
    "math/03": [
        ("bilinear_interpolation.png", diagram_bilinear_interpolation),
    ],
    "math/05": [
        ("matrix_basis_vectors.png", diagram_matrix_basis_vectors),
    ],
    "math/06": [
        ("frustum.png", diagram_frustum),
    ],
    "gpu/04": [
        ("uv_mapping.png", diagram_uv_mapping),
        ("filtering_comparison.png", diagram_filtering_comparison),
    ],
}

# Full lesson directory names for display
LESSON_NAMES = {
    "math/01": "math/01-vectors",
    "math/03": "math/03-bilinear-interpolation",
    "math/05": "math/05-matrices",
    "math/06": "math/06-projections",
    "gpu/04": "gpu/04-textures-and-samplers",
}


def match_lesson(query):
    """Match a query like 'math/01', 'math/01-vectors', or '01' to a registry key."""
    q = query.strip().rstrip("/")

    # Exact match
    if q in DIAGRAMS:
        return q

    # Match by full name
    for key, name in LESSON_NAMES.items():
        if q == name:
            return key

    # Match by number suffix (e.g. "01" matches "math/01")
    for key in DIAGRAMS:
        num = key.split("/")[1]
        if q == num:
            return key

    return None


def main():
    parser = argparse.ArgumentParser(
        description="Generate matplotlib diagrams for forge-gpu lessons."
    )
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument(
        "--lesson", help="Lesson to generate diagrams for (e.g. math/01)"
    )
    group.add_argument("--all", action="store_true", help="Generate all diagrams")
    group.add_argument("--list", action="store_true", help="List available diagrams")
    args = parser.parse_args()

    if args.list:
        print("Available diagrams:")
        for key in sorted(DIAGRAMS.keys()):
            name = LESSON_NAMES.get(key, key)
            diagrams = DIAGRAMS[key]
            print(f"\n  {name}/")
            for filename, _ in diagrams:
                print(f"    {filename}")
        total = sum(len(d) for d in DIAGRAMS.values())
        print(f"\n{total} diagrams total.")
        return 0

    if args.all:
        keys = sorted(DIAGRAMS.keys())
    else:
        key = match_lesson(args.lesson)
        if key is None:
            print(f"No diagrams registered for '{args.lesson}'.")
            print("Use --list to see available diagrams.")
            return 1
        keys = [key]

    total = 0
    for key in keys:
        name = LESSON_NAMES.get(key, key)
        print(f"{name}/")
        for _, func in DIAGRAMS[key]:
            func()
            total += 1

    print(f"\nGenerated {total} diagram(s).")
    return 0


if __name__ == "__main__":
    sys.exit(main())
