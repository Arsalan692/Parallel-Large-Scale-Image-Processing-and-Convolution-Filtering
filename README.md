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
scripts/           build.ps1 (compile everything), run.ps1 (run, optionally with mpiexec)
analysis/          Python scripts that turn results into graphs
results/           benchmark results (CSV)
report/            final report (.docx) and figures
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

# run a program (examples, available after their phase is done)
powershell -ExecutionPolicy Bypass -File scripts\run.ps1 sequential --count 100 --save 5
powershell -ExecutionPolicy Bypass -File scripts\run.ps1 -Procs 4 mpi_batch --count 1000 --threads 2
```
Each program accepts `--help` to list its options (`--input`, `--count`, `--save`, `--out`, `--csv`, `--threads`, `--schedule`, `--mode`, `--label`).

## Status
- [x] Phase 0: toolchain, dataset, repo
- [x] Phase 1: project skeleton, shared code, build/run scripts, unit tests
- [x] Phase 2: sequential baseline (10,000 images: 212.8 s)
- [x] Phase 3: OpenMP version (best: 8 threads, images mode, 56.9 s = 3.74x)
- [x] Phase 4: MPI versions (batch: 8 procs 54.9 s = 3.88x; strips: best 4 procs 141.3 s = 1.51x)
- [x] Phase 5: hybrid MPI + OpenMP (batch best 1x8 50.1 s = 4.25x; strips best 1x8 132.7 s)
- [x] Phase 6: final benchmark, 27 configs x 2 repeats on 10,000 images, all checksums identical (`results/benchmarks.csv`; seq 195.1 s, best OpenMP images 8 thr 55.3 s = 3.53x, MPI batch 8 procs 55.7 s = 3.50x)
  - run it: `powershell -ExecutionPolicy Bypass -File scripts\benchmark.ps1` (about 2 h; resumable)
- [ ] Phase 7: analysis and graphs
- [ ] Phase 8: report
- [ ] Phase 9: submission and viva prep
