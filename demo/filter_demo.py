"""
Demo: what "applying a convolution filter" means.
Takes one photo and produces blurred, sharpened and edge-detected versions,
using a hand-written convolution (the same idea we will write in C++ later).
"""
import sys
from pathlib import Path
import numpy as np
from PIL import Image
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

HERE = Path(__file__).parent
src = Path(sys.argv[1]) if len(sys.argv) > 1 else HERE / "input" / "sample.jpg"
out_dir = HERE / "output"
out_dir.mkdir(exist_ok=True)

# ---------------- The kernels (small grids of numbers) ----------------
GAUSSIAN_5x5 = np.outer([1, 4, 6, 4, 1], [1, 4, 6, 4, 1]) / 256.0
SHARPEN_3x3 = np.array([[ 0, -1,  0],
                        [-1,  5, -1],
                        [ 0, -1,  0]], dtype=float)
SOBEL_X = np.array([[-1, 0, 1],
                    [-2, 0, 2],
                    [-1, 0, 1]], dtype=float)
SOBEL_Y = SOBEL_X.T


def convolve(channel: np.ndarray, kernel: np.ndarray) -> np.ndarray:
    """For every pixel: multiply its neighbourhood by the kernel and sum.
    Borders are handled by repeating the edge pixels (padding)."""
    k = kernel.shape[0] // 2
    padded = np.pad(channel, k, mode="edge")
    h, w = channel.shape
    out = np.zeros((h, w), dtype=float)
    # Loop over kernel cells; each step adds (kernel value x shifted image).
    # Mathematically identical to looping over every pixel, just faster in Python.
    for dy in range(kernel.shape[0]):
        for dx in range(kernel.shape[1]):
            out += kernel[dy, dx] * padded[dy:dy + h, dx:dx + w]
    return out


def apply_rgb(img: np.ndarray, kernel: np.ndarray) -> np.ndarray:
    return np.stack([convolve(img[..., c], kernel) for c in range(3)], axis=-1)


def to_u8(a: np.ndarray) -> np.ndarray:
    return np.clip(a, 0, 255).astype(np.uint8)


img = np.asarray(Image.open(src).convert("RGB"), dtype=float)
gray = img @ np.array([0.299, 0.587, 0.114])

blur = to_u8(apply_rgb(img, GAUSSIAN_5x5))
sharp = to_u8(apply_rgb(img, SHARPEN_3x3))
gx, gy = convolve(gray, SOBEL_X), convolve(gray, SOBEL_Y)
edges = to_u8(np.hypot(gx, gy))

original = img.astype(np.uint8)
Image.fromarray(blur).save(out_dir / "sample_blur.jpg", quality=95)
Image.fromarray(sharp).save(out_dir / "sample_sharp.jpg", quality=95)
Image.fromarray(edges).save(out_dir / "sample_edges.jpg", quality=95)

# ---------------- Comparison sheet: full images + zoomed crops ----------------
h, w = gray.shape
cy, cx, s = int(h * 0.45), int(w * 0.40), 160      # zoom window
crop = lambda a: a[cy:cy + s, cx:cx + s]

panels = [("Original", original, None),
          ("Gaussian Blur (5x5)", blur, None),
          ("Sharpen (3x3)", sharp, None),
          ("Edge Detection (Sobel)", edges, "gray")]

fig, axes = plt.subplots(2, 4, figsize=(20, 9.5))
for i, (title, im, cmap) in enumerate(panels):
    axes[0, i].imshow(im, cmap=cmap)
    axes[0, i].set_title(title, fontsize=15, fontweight="bold")
    axes[0, i].add_patch(plt.Rectangle((cx, cy), s, s, fill=False, ec="red", lw=2))
    axes[1, i].imshow(crop(im), cmap=cmap, interpolation="nearest")
    axes[1, i].set_title("Zoomed (red box)", fontsize=12)
    for r in (0, 1):
        axes[r, i].axis("off")
fig.suptitle("Same photo, 3 convolution filters  (every one of the "
             f"{h * w:,} pixels recomputed per filter)", fontsize=16)
fig.tight_layout()
fig.savefig(out_dir / "comparison.png", dpi=80)
print("saved:", out_dir / "comparison.png")
