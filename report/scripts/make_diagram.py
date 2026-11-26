"""Draws the decomposition diagram for the Methodology section:
left = MPI batch (whole images per process), right = MPI strips (one image split into
horizontal strips, with halo rows exchanged between neighbours).
Output: report/figures/decomposition.png

Run:  D:\\Python\\python.exe report\\scripts\\make_diagram.py
"""
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.patches import FancyArrowPatch, FancyBboxPatch, Rectangle

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "report" / "figures" / "decomposition.png"

INK, INK2, LINE = "#191919", "#666666", "#c2c2c2"
PROC = ["#257de0", "#f47230", "#00b894", "#f2a600"]  # one colour per MPI process
plt.rcParams.update({"font.family": ["Segoe UI", "DejaVu Sans"], "font.size": 10})

fig, (a1, a2) = plt.subplots(1, 2, figsize=(10, 4.6), gridspec_kw={"width_ratios": [1, 1]})
for ax in (a1, a2):
    ax.set_xlim(0, 10)
    ax.set_ylim(0, 10)
    ax.axis("off")


def arrow(ax, p, q, text=None, side="right"):
    ax.add_patch(FancyArrowPatch(p, q, arrowstyle="-|>", mutation_scale=11, color=INK2, lw=1.1))
    if text:
        mx, my = (p[0] + q[0]) / 2, (p[1] + q[1]) / 2
        ax.text(mx + (0.15 if side == "right" else -0.15), my, text, fontsize=8.5, color=INK2,
                ha="left" if side == "right" else "right", va="center")


def proc_box(ax, x, y, w, h, color, label, sub):
    ax.add_patch(FancyBboxPatch((x, y), w, h, boxstyle="round,pad=0,rounding_size=0.18",
                                fc="white", ec=color, lw=1.8))
    ax.text(x + w / 2, y + h * 0.64, label, ha="center", va="center", fontsize=9.5, color=INK, weight="semibold")
    ax.text(x + w / 2, y + h * 0.28, sub, ha="center", va="center", fontsize=8, color=INK2)


# ---- Left: batch distribution -------------------------------------------------------------
a1.set_title("(a) MPI batch: whole images per process", loc="left", fontsize=11, weight="semibold", color=INK)
a1.text(0.2, 9.0, "10,000 images (block partition, 2,500 each with P = 4)", fontsize=8.5, color=INK2)
n_tiles, tile_w, gap, x0, y0 = 20, 0.42, 0.06, 0.3, 7.7
for i in range(n_tiles):
    a1.add_patch(Rectangle((x0 + i * (tile_w + gap), y0), tile_w, 0.85, fc=PROC[i // 5], ec="white", lw=0))
for p in range(4):
    bx = 0.25 + p * 2.45
    block_mid = x0 + (p * 5 + 2.5) * (tile_w + gap) - gap / 2
    arrow(a1, (block_mid, y0 - 0.1), (bx + 1.0, 5.25))
    proc_box(a1, bx, 3.9, 2.0, 1.3, PROC[p], f"Rank {p}", "T threads")
    a1.text(bx + 1.0, 3.35, "read → filter\n→ checksum", ha="center", va="top", fontsize=8, color=INK2)
a1.add_patch(FancyBboxPatch((2.6, 0.45), 4.8, 0.9, boxstyle="round,pad=0,rounding_size=0.15", fc="#f6f6f6", ec=LINE))
a1.text(5.0, 0.9, "MPI_Reduce(checksum) → rank 0", ha="center", va="center", fontsize=9, color=INK)
a1.text(5.0, 6.45, "MPI_Bcast sends the file list; each rank takes its own block", fontsize=8.5,
        color=INK2, ha="center", va="center", bbox=dict(fc="white", ec="none", pad=1.5))
for p in range(4):
    arrow(a1, (0.25 + p * 2.45 + 1.0, 2.55), (5.0 + (p - 1.5) * 0.9, 1.4))

# ---- Right: strip decomposition with halo -------------------------------------------------
a2.set_title("(b) MPI strips: one image, P strips + halo rows", loc="left", fontsize=11, weight="semibold", color=INK)
ix, iy, iw, ih = 0.4, 0.7, 4.2, 8.0
strip_h = ih / 4
for p in range(4):
    y = iy + ih - (p + 1) * strip_h
    a2.add_patch(Rectangle((ix, y), iw, strip_h, fc=PROC[p], ec="white", lw=2, alpha=0.9))
    a2.text(ix + iw / 2, y + strip_h / 2, f"rows of rank {p}", ha="center", va="center", fontsize=9,
            color="#0b0b0b" if PROC[p] == "#f2a600" else "white", weight="semibold")
    if p < 3:  # halo band around each internal boundary
        by = y
        a2.add_patch(Rectangle((ix - 0.12, by - 0.22), iw + 0.24, 0.44, fc="none", ec=INK, lw=1, ls=(0, (3, 2))))
a2.text(ix, iy - 0.45, "1024 × 1024 X-ray, rank 0 reads it", fontsize=8.5, color=INK2)

steps = [("1  MPI_Bcast", "image size"),
         ("2  MPI_Scatterv", "each rank gets its rows"),
         ("3  MPI_Sendrecv × 2", "swap 2 halo rows with\nthe neighbour above / below"),
         ("4  #pragma omp parallel for", "threads filter the strip"),
         ("5  MPI_Gatherv × 3", "rank 0 assembles blur,\nsharpen and edge images")]
sy = 8.4
for head, sub in steps:
    a2.text(5.15, sy, head, fontsize=9, color=INK, weight="semibold", va="top")
    a2.text(5.45, sy - 0.48, sub, fontsize=8.2, color=INK2, va="top")
    sy -= 1.62 if "\n" in sub else 1.3
a2.text(ix + iw + 0.2, iy + ih - strip_h - 0.0, "halo", fontsize=8, color=INK2, va="center")

fig.tight_layout(w_pad=2.5)
fig.savefig(OUT, dpi=220, bbox_inches="tight", facecolor="white")
print("wrote", OUT)
