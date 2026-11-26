"""Builds report/figures/sample_filters.png: one X-ray and its three filtered outputs
(written by: scripts/run.ps1 sequential --save N)."""
import sys
from pathlib import Path

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
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

fig, axes = plt.subplots(2, 4, figsize=(18, 9.6))
for i, (title, path) in enumerate(panels):
    img = Image.open(path).convert("L")
    axes[0, i].imshow(img, cmap="gray", vmin=0, vmax=255)
    axes[0, i].set_title(title, fontsize=14, fontweight="bold")
    axes[0, i].add_patch(plt.Rectangle((x0, y0), s, s, fill=False, ec="red", lw=2))
    axes[1, i].imshow(img.crop((x0, y0, x0 + s, y0 + s)), cmap="gray", vmin=0, vmax=255,
                      interpolation="nearest")
    axes[1, i].set_title("Zoomed (red box)", fontsize=11)
    for r in (0, 1):
        axes[r, i].axis("off")
fig.suptitle(f"NIH ChestX-ray14 image {stem}.png (1024x1024) and its three filtered outputs", fontsize=15)
fig.tight_layout(rect=(0, 0, 1, 0.95))
OUT.parent.mkdir(parents=True, exist_ok=True)
fig.savefig(OUT, dpi=90)
print("saved", OUT)
