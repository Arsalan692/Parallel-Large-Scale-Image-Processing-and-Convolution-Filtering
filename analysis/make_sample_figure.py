"""Builds report/figures/sample_filters.png: one X-ray and its three filtered outputs
(written by: scripts/run.ps1 sequential --save N).
Top row: full images (the yellow box marks the zoomed region); bottom row: the zoomed region."""
import sys
from pathlib import Path

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

plt.rcParams["font.family"] = ["Segoe UI", "DejaVu Sans"]
from PIL import Image

ROOT = Path(__file__).resolve().parent.parent
DATASET = Path("E:/X-Ray Dataset/images")
SAMPLES = ROOT / "output" / "sequential"
OUT = ROOT / "report" / "figures" / "sample_filters.png"

stem = sys.argv[1] if len(sys.argv) > 1 else sorted(SAMPLES.glob("*_blur.png"))[0].name[: -len("_blur.png")]
panels = [
    ("Original X-ray", DATASET / f"{stem}.png"),
    ("Gaussian blur (5x5)", SAMPLES / f"{stem}_blur.png"),
    ("Sharpen (3x3)", SAMPLES / f"{stem}_sharpen.png"),
    ("Sobel edge detection", SAMPLES / f"{stem}_edges.png"),
]

# Zoom window (centre region) to make blur/sharpen differences visible.
y0, x0, s = 380, 300, 260

fig, axes = plt.subplots(2, 4, figsize=(16, 8.4), gridspec_kw={"hspace": 0.04, "wspace": 0.04})
for i, (title, path) in enumerate(panels):
    img = Image.open(path).convert("L")
    axes[0, i].imshow(img, cmap="gray", vmin=0, vmax=255)
    axes[0, i].set_title(title, fontsize=15, fontweight="semibold", color="#191919", pad=8)
    axes[0, i].add_patch(plt.Rectangle((x0, y0), s, s, fill=False, ec="#f2a600", lw=2))
    axes[1, i].imshow(img.crop((x0, y0, x0 + s, y0 + s)), cmap="gray", vmin=0, vmax=255,
                      interpolation="nearest")
    for r in (0, 1):
        axes[r, i].axis("off")
fig.subplots_adjust(left=0.005, right=0.995, top=0.94, bottom=0.005)
OUT.parent.mkdir(parents=True, exist_ok=True)
fig.savefig(OUT, dpi=150, facecolor="white")
print("saved", OUT)
