# Hybrid Parallel Large-Scale Image Processing and Convolution Filtering

Semester project for **Parallel and Distributed Computing (CS3006)**, FAST-NUCES Karachi, Fall 2026.

| Member | Roll No. |
|---|---|
| Muhammad Yousuf (Group Leader) | 23K-0690 |
| Arsalan Ahmed | 23K-0536 |

## Overview
We apply three convolution filters (**Gaussian blur 5×5, sharpen 3×3 and Sobel edge detection**) to a large batch of medical X-ray images. We then measure how much faster the work runs when it is parallelized with **OpenMP**, **MPI** and **hybrid MPI + OpenMP**, compared with a sequential baseline.

The convolution is written and parallelized by hand (`src/common/convolution.cpp`). OpenCV is used only to read and write image files.

See [PLAN.md](PLAN.md) for the phase-by-phase plan.

## Dataset
**NIH ChestX-ray14**: 1024×1024 grayscale chest X-rays from the NIH Clinical Center. We downloaded 14,999 images (archives `images_001` and `images_002`) and use 10,000 in the main experiments.

The dataset is not stored in this repo because of its size.

> Wang X, Peng Y, Lu L, Lu Z, Bagheri M, Summers RM. *ChestX-ray8: Hospital-scale Chest X-ray Database and Benchmarks on Weakly-Supervised Classification and Localization of Common Thorax Diseases.* IEEE CVPR 2017.

## Repository layout
```
src/
  common/          shared code used by EVERY version
    convolution.*    hand-written convolution + the 3 filters (row by row)
    kernels.hpp      Gaussian, sharpen and Sobel kernels
    partition.hpp    block partitioning of images/rows among workers
    image_io.*       read/write PNG (the only place OpenCV is used)
    checksum.hpp     output fingerprints (to prove parallel output == sequential output)
    options.*        command-line options
    results.*        timing summary + CSV output
    mpi_utils.hpp    MPI helpers: broadcast file list, per-process breakdown table
    timer.hpp, image.hpp, config.hpp
  sequential/      Phase 2: single-core baseline
  openmp/          Phase 3: OpenMP threads (--mode rows | images, --schedule static | dynamic)
  mpi_batch/       Phase 4A: MPI, whole images distributed across processes (+ OpenMP = hybrid)
  mpi_strips/      Phase 4B: MPI, each image split into strips with halo exchange (+ OpenMP = hybrid)
tests/             unit tests (test_common) + validation against OpenCV (validate_opencv)
scripts/           build.ps1 (compile everything), run.ps1 (run, optionally with mpiexec),
                   benchmark.ps1 (all experiments), make_submission.py (submission package)
analysis/          Python scripts that turn results into graphs
results/           benchmark results (CSV)
report/            final report (.docx + .pdf), figures, and the scripts that build them
viva/              plain-language project explanation and viva guide (PDF)
demo/              Python demo showing what each filter does to one photo
```

## Toolchain (Windows)
- MSYS2 MinGW-w64 `g++` 16 with OpenMP
- Microsoft MPI (MS-MPI) 10.1 (runtime + MSYS2 `mingw-w64-x86_64-msmpi` headers)
- OpenCV 5 (MSYS2 `mingw-w64-x86_64-opencv`)
- Python 3.13 venv for analysis: `python -m venv venv` then `venv\Scripts\pip install -r requirements.txt`

## Build and run
```powershell
# build everything
powershell -ExecutionPolicy Bypass -File scripts\build.ps1

# run the unit tests
powershell -ExecutionPolicy Bypass -File scripts\run.ps1 test_common

# run the programs (the dataset folder can be changed with --input)
powershell -ExecutionPolicy Bypass -File scripts\run.ps1 sequential --count 1000
powershell -ExecutionPolicy Bypass -File scripts\run.ps1 openmp --count 1000 --threads 8 --mode images
powershell -ExecutionPolicy Bypass -File scripts\run.ps1 -Procs 4 mpi_batch --count 1000 --threads 2
powershell -ExecutionPolicy Bypass -File scripts\run.ps1 -Procs 4 mpi_strips --count 200

# full benchmark: 27 configurations x 2 repeats on 10,000 images (about 2 h, resumable)
powershell -ExecutionPolicy Bypass -File scripts\benchmark.ps1
```
Each program accepts `--help` to list its options (`--input`, `--count`, `--save`, `--out`, `--csv`, `--threads`, `--schedule`, `--mode`, `--label`), and prints its run time and an output checksum. For the same `--count`, every version prints the same checksum.

## Results
10,000 images. Each value is the mean of 2 runs on an Intel i7-8565U (4 cores / 8 threads). Full table: [results/summary.md](results/summary.md); raw data: [results/benchmarks.csv](results/benchmarks.csv).

| Version | Workers | Time | Speedup |
|---|---|---|---|
| Sequential | 1 | 195.1 s | 1.00× |
| OpenMP, images mode | 8 threads | **55.3 s** | **3.53×** |
| MPI batch | 8 processes | 55.7 s | 3.50× |
| Hybrid batch (1×8 / 2×4 / 4×2) | 8 | 56.7–57.1 s | ≈3.43× |
| OpenMP, rows mode | 8 threads | 122.4 s | 1.59× |
| Hybrid strips (1×8) | 8 | 127.3 s | 1.53× |
| MPI strips | 4 / 8 processes | 141.0 / 170.4 s | 1.38× / 1.14× |

All 54 benchmark runs produced the same output checksum (`f3eee2e41bf6a7ce`) as the sequential program.

## Status
- [x] Phase 0: toolchain, dataset, repo
- [x] Phase 1: project skeleton, shared code, build/run scripts, unit tests (9/9 pass)
- [x] Phase 2: sequential baseline + validation against OpenCV
- [x] Phase 3: OpenMP version (rows and images modes, static/dynamic scheduling)
- [x] Phase 4: MPI versions (batch distribution; strip decomposition with halo exchange)
- [x] Phase 5: hybrid MPI + OpenMP
- [x] Phase 6: final benchmark, 27 configurations × 2 repeats (`scripts/benchmark.ps1` → `results/benchmarks.csv`)
- [x] Phase 7: analysis and graphs (`analysis/make_graphs.py` → `report/figures/`, `results/summary.md`)
- [x] Phase 8: report (`report/PDC_Project_Report_Group18.docx`; rebuild with `report/scripts/build_report.js`)
- [x] Phase 9: submission package (`scripts/make_submission.py`) and viva guide (`viva/Project_Explained_Group18.pdf`)

The per-phase numbers in [PLAN.md](PLAN.md) for Phases 2–5 come from early single runs. The final, repeated measurements are the ones above.
