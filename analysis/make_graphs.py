"""Phase 7 - turns results/benchmarks.csv (Phase 6, 27 configurations x 2 repeats) into
the report's graphs and a summary table.

Outputs
    report/figures/fig1_time.png        execution time vs number of workers
    report/figures/fig2_speedup.png     speedup vs workers (+ ideal line)
    report/figures/fig3_efficiency.png  efficiency vs workers
    report/figures/fig4_hybrid.png      hybrid P x T splits, batch vs strips
    report/figures/fig5_breakdown.png   where the time goes: read / compute / comm / other
    report/figures/fig6_amdahl.png      measured speedup vs Amdahl's law prediction
    results/summary.csv, results/summary.md   mean/min/max, speedup, efficiency per config

Run:  D:\\Python\\python.exe analysis\\make_graphs.py
"""
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import pandas as pd

ROOT = Path(__file__).resolve().parent.parent
CSV = ROOT / "results" / "benchmarks.csv"
FIG = ROOT / "report" / "figures"
FIG.mkdir(parents=True, exist_ok=True)

# ---- Style (validated categorical palette; text never wears a series colour) ----------
INK, INK2, GRID, AXIS = "#191919", "#666666", "#e7e7e7", "#c2c2c2"
C_BATCH, C_STRIPS, C_IMAGES, C_ROWS = "#257de0", "#f47230", "#00b894", "#f2a600"
C_READ, C_COMPUTE, C_COMM, C_OTHER = "#6a4ce2", "#008a17", "#ee7fac", "#c9c9c9"
SERIES = {  # entity -> (label, colour, marker); the colour follows the entity in every figure
    "batch": ("MPI batch (whole images per process)", C_BATCH, "o"),
    "strips": ("MPI strips (halo exchange)", C_STRIPS, "s"),
    "images": ("OpenMP images mode", C_IMAGES, "D"),
    "rows": ("OpenMP rows mode", C_ROWS, "^"),
}
plt.rcParams.update({
    "font.family": ["Segoe UI", "DejaVu Sans"], "font.size": 10,
    "text.color": INK, "axes.labelcolor": INK2, "xtick.color": INK2, "ytick.color": INK2,
    "axes.edgecolor": AXIS, "axes.linewidth": 1, "axes.spines.top": False, "axes.spines.right": False,
    "axes.grid": True, "grid.color": GRID, "grid.linewidth": 0.8, "axes.axisbelow": True,
    "axes.titlesize": 12, "axes.titleweight": "semibold", "axes.titlelocation": "left",
    "legend.frameon": False, "legend.fontsize": 9, "figure.dpi": 100, "savefig.dpi": 200,
    "lines.linewidth": 2, "lines.solid_capstyle": "round", "lines.solid_joinstyle": "round",
})

# ---- Load and summarise ----------------------------------------------------------------
df = pd.read_csv(CSV)
assert df["checksum"].nunique() == 1, "not every run produced the same output!"
df["id"] = df["label"].str.split(":").str[1]
df["other_s"] = df["total_s"] - df[["read_s", "compute_s", "comm_s", "write_s"]].sum(axis=1)

g = df.groupby("id", sort=False)
s = g.agg(version=("version", "first"), mode=("mode", "first"), procs=("procs", "first"),
          threads=("threads", "first"), workers=("workers", "first"), runs=("total_s", "size"),
          mean_s=("total_s", "mean"), min_s=("total_s", "min"), max_s=("total_s", "max"),
          read_s=("read_s", "mean"), compute_s=("compute_s", "mean"), comm_s=("comm_s", "mean"),
          other_s=("other_s", "mean"))
T_SEQ = s.loc["seq", "mean_s"]
s["speedup"] = T_SEQ / s["mean_s"]
s["efficiency_pct"] = 100 * s["speedup"] / s["workers"]
s["spread_pct"] = 100 * (s["max_s"] - s["min_s"]) / s["mean_s"]
s.round(3).to_csv(ROOT / "results" / "summary.csv")

with open(ROOT / "results" / "summary.md", "w", encoding="utf-8") as f:
    f.write(f"# Benchmark summary (10,000 images, mean of {int(s['runs'].min())} runs, "
            f"sequential = {T_SEQ:.1f} s, checksum {df['checksum'].iloc[0]})\n\n")
    f.write("| Config | P | T | Workers | Time (s) | Min–max (s) | Speedup | Efficiency | "
            "Read | Compute | Comm |\n|---|---|---|---|---|---|---|---|---|---|---|\n")
    for cid, r in s.iterrows():
        f.write(f"| {cid} | {r.procs} | {r.threads} | {r.workers} | {r.mean_s:.1f} | "
                f"{r.min_s:.1f}–{r.max_s:.1f} | {r.speedup:.2f}× | {r.efficiency_pct:.0f}% | "
                f"{r.read_s:.1f} | {r.compute_s:.1f} | {r.comm_s:.1f} |\n")

W = [1, 2, 4, 8]
SCALING = {  # entity -> config id for 1, 2, 4, 8 workers (pure OpenMP or pure MPI)
    "batch": [f"batch-{p}x1" for p in W],
    "strips": [f"strips-{p}x1" for p in W],
    "images": [f"omp-images-{t}" for t in W],
    "rows": [f"omp-rows-{t}" for t in W],
}


def workers_axis(ax):
    ax.set_xscale("log", base=2)
    ax.set_xticks(W, [str(w) for w in W])
    ax.minorticks_off()
    ax.set_xlim(0.85, 9.6)
    ax.set_xlabel("Workers (OpenMP threads, or MPI processes)")
    # Workers 5-8 are hyper-threads on the 4-core i7-8565U
    ax.axvspan(4, 9.6, color="#f3f3f3", zorder=0, lw=0)
    ax.text(5.9, 0.03, "hyper-threads\n(only 4 physical cores)", transform=ax.get_xaxis_transform(),
            ha="center", va="bottom", fontsize=8, color=INK2)


def plot_series(ax, column, order=("batch", "images", "strips", "rows")):
    for key in order:
        label, color, marker = SERIES[key]
        ys = s.loc[SCALING[key], column].to_numpy()
        # batch and images-mode results are almost identical: draw batch wider and underneath
        # (bigger marker, thicker line) so both stay visible where they overlap
        big = key == "batch"
        ax.plot(W, ys, color=color, marker=marker, markersize=11 if big else 6.5, markeredgecolor="white",
                markeredgewidth=1.5, lw=4.5 if big else 2, label=label, zorder=2.5 if big else 3)


def end_label(ax, y, text, dy=0):
    ax.annotate(text, (8, y), xytext=(8, dy), textcoords="offset points", va="center",
                fontsize=9, color=INK)


def save(fig, name):
    fig.tight_layout()
    fig.savefig(FIG / name, bbox_inches="tight", facecolor="white")
    plt.close(fig)
    print("wrote", FIG / name)


# ---- Fig 1: execution time -------------------------------------------------------------
fig, ax = plt.subplots(figsize=(7.2, 4.4))
workers_axis(ax)
ax.axhline(T_SEQ, color=INK2, lw=1, ls=(0, (4, 3)), zorder=2)
ax.text(2.8, T_SEQ + 3, f"sequential {T_SEQ:.1f} s", fontsize=8.5, color=INK2, va="bottom")
plot_series(ax, "mean_s")
ax.set_ylim(0, 215)
ax.set_ylabel("Execution time (s)")
ax.set_title("Execution time on 10,000 X-rays")
for key, dy in (("strips", 0), ("rows", 0)):
    end_label(ax, s.loc[SCALING[key][-1], "mean_s"], f"{s.loc[SCALING[key][-1], 'mean_s']:.0f} s", dy)
end_label(ax, s.loc["omp-images-8", "mean_s"], "55–56 s\n(batch ≈ images)")
ax.legend(loc="upper center", bbox_to_anchor=(0.5, -0.16), ncol=2)
save(fig, "fig1_time.png")

# ---- Fig 2: speedup --------------------------------------------------------------------
fig, ax = plt.subplots(figsize=(7.2, 4.4))
workers_axis(ax)
ax.plot(W, W, color=AXIS, lw=1.2, ls=(0, (4, 3)), zorder=2)
ax.text(2.9, 3.2, "ideal (linear)", rotation=0, fontsize=8.5, color=INK2)
plot_series(ax, "speedup")
ax.set_ylim(0, 8.4)
ax.set_ylabel(f"Speedup  (T_sequential / T_parallel)")
ax.set_title("Speedup over the sequential version")
end_label(ax, s.loc["omp-images-8", "speedup"], "3.5×")
end_label(ax, s.loc["omp-rows-8", "speedup"], f"{s.loc['omp-rows-8', 'speedup']:.2f}×", 3)
end_label(ax, s.loc["strips-8x1", "speedup"], f"{s.loc['strips-8x1', 'speedup']:.2f}×", -5)
ax.legend(loc="upper center", bbox_to_anchor=(0.5, -0.16), ncol=2)
save(fig, "fig2_speedup.png")

# ---- Fig 3: efficiency -----------------------------------------------------------------
fig, ax = plt.subplots(figsize=(7.2, 4.4))
workers_axis(ax)
plot_series(ax, "efficiency_pct")
ax.set_ylim(0, 110)
ax.set_ylabel("Efficiency (%)  = speedup / workers")
ax.set_title("Parallel efficiency")
end_label(ax, s.loc["omp-images-8", "efficiency_pct"], "44%")
ax.legend(loc="upper center", bbox_to_anchor=(0.5, -0.16), ncol=2)
save(fig, "fig3_efficiency.png")

# ---- Fig 4: hybrid splits (8 workers) --------------------------------------------------
splits = ["1x8", "2x4", "4x2", "8x1"]
fig, ax = plt.subplots(figsize=(7.2, 4.2))
ax.grid(axis="x", visible=False)
xs = range(len(splits))
bw = 0.34
for off, key in ((-bw / 2 - 0.01, "batch"), (bw / 2 + 0.01, "strips")):
    label, color, _ = SERIES[key]
    vals = [s.loc[f"{key}-{sp}", "mean_s"] for sp in splits]
    bars = ax.bar([x + off for x in xs], vals, width=bw, color=color, label=label, zorder=3)
    for b, v in zip(bars, vals):
        ax.text(b.get_x() + b.get_width() / 2, v + 2, f"{v:.0f} s", ha="center", va="bottom",
                fontsize=9, color=INK)
ax.set_xticks(list(xs), [f"{sp.replace('x', ' × ')}" for sp in splits])
ax.set_xlabel("MPI processes × OpenMP threads per process  (always 8 workers)")
ax.set_ylabel("Execution time (s)")
ax.set_ylim(0, 200)
ax.set_title("Hybrid MPI + OpenMP: same 8 workers, different splits")
ax.legend(loc="upper left")
save(fig, "fig4_hybrid.png")

# ---- Fig 5: time breakdown -------------------------------------------------------------
rows = [("seq", "Sequential"), ("omp-rows-8", "OpenMP rows, 8 thr"),
        ("omp-images-8", "OpenMP images, 8 thr"), ("batch-8x1", "MPI batch, 8 × 1"),
        ("batch-1x8", "Hybrid batch, 1 × 8"), ("strips-4x1", "MPI strips, 4 × 1"),
        ("strips-8x1", "MPI strips, 8 × 1"), ("strips-1x8", "Hybrid strips, 1 × 8")]
parts = [("read_s", "Read (PNG decode)", C_READ, "white"), ("compute_s", "Compute (3 filters)", C_COMPUTE, "white"),
         ("comm_s", "MPI communication", C_COMM, "white"), ("other_s", "Other (checksum, waiting)", C_OTHER, INK)]
fig, ax = plt.subplots(figsize=(7.4, 4.6))
ax.grid(axis="y", visible=False)
ys = list(range(len(rows)))[::-1]
for (cid, _), y in zip(rows, ys):
    left = 0.0
    for col, _, color, txt in parts:
        v = max(s.loc[cid, col], 0.0)
        ax.barh(y, v, left=left, height=0.6, color=color, edgecolor="white", linewidth=1.5, zorder=3)
        if v >= 12:
            ax.text(left + v / 2, y, f"{v:.0f}", ha="center", va="center", fontsize=8.5, color=txt, zorder=4)
        left += v
    ax.text(left + 2, y, f"{s.loc[cid, 'mean_s']:.0f} s", va="center", fontsize=9, color=INK)
ax.set_yticks(ys, [r[1] for r in rows])
ax.set_xlim(0, 215)
ax.set_xlabel("Time (s) as measured on rank 0 / averaged over threads")
ax.set_title("Where the time goes")
handles = [plt.Rectangle((0, 0), 1, 1, color=c) for _, _, c, _ in parts]
ax.legend(handles, [p[1] for p in parts], loc="upper center", bbox_to_anchor=(0.42, -0.13), ncol=4, fontsize=8.5)
save(fig, "fig5_breakdown.png")

# ---- Fig 6: Amdahl's law ---------------------------------------------------------------
f_serial = s.loc["seq", "read_s"] / T_SEQ  # reading is the part rows mode / strips cannot parallelise
fig, ax = plt.subplots(figsize=(7.2, 4.4))
workers_axis(ax)
pp = [1 + i * 0.05 for i in range(141)]
ax.plot(pp, [1 / (f_serial + (1 - f_serial) / p) for p in pp], color=INK2, lw=1.4, ls=(0, (4, 3)),
        zorder=2, label=f"Amdahl prediction, serial fraction f = {f_serial:.2f}")
ax.axhline(1 / f_serial, color=AXIS, lw=1, zorder=1)
ax.text(0.9, 1 / f_serial + 0.06, f"Amdahl ceiling 1/f = {1 / f_serial:.2f}×", fontsize=8.5, color=INK2)
for key in ("images", "rows"):
    label, color, marker = SERIES[key]
    ax.plot(W, s.loc[SCALING[key], "speedup"], color=color, marker=marker, markersize=7,
            markeredgecolor="white", markeredgewidth=1.5, label=label + " (measured)", zorder=3)
ax.set_ylim(0, 4)
ax.annotate("images mode reads in parallel too,\nso it is not capped", (4, s.loc["omp-images-4", "speedup"]),
            xytext=(1.25, 3.3), fontsize=8.5, color=INK2, arrowprops=dict(arrowstyle="-", color=AXIS, lw=1))
ax.set_ylabel("Speedup")
ax.set_title("Amdahl's law: a sequential read caps the speedup")
ax.legend(loc="upper center", bbox_to_anchor=(0.5, -0.16), ncol=1)
save(fig, "fig6_amdahl.png")

print(f"\nsequential {T_SEQ:.1f} s, serial (read) fraction {f_serial:.3f}, "
      f"summary -> results/summary.csv and results/summary.md")
