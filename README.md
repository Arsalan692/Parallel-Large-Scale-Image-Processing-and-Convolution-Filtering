# Hybrid Parallel Large-Scale Image Processing and Convolution Filtering

Semester project for **Parallel and Distributed Computing (CS3006)**, FAST-NUCES Karachi, Fall 2026.

| Member | Roll No. |
|---|---|
| Muhammad Yousuf (Group Leader) | 23K-0690 |
| Arsalan Ahmed | 23K-0536 |

## Overview
We apply three convolution filters (**Gaussian blur, sharpening and Sobel edge detection**) to a large batch of medical X-ray images. We then measure how much faster the work runs when it is parallelized with **MPI**, **OpenMP** and a **hybrid MPI + OpenMP** approach, compared with a sequential baseline.

The convolution loops are written and parallelized by hand. OpenCV is used only to load and save images.

## Dataset
**NIH ChestX-ray14**: 1024×1024 grayscale chest X-rays from the NIH Clinical Center. We use about 15,000 images (archives `images_001` and `images_002`).

The dataset is not stored in this repo because it is too large.

> Wang X, Peng Y, Lu L, Lu Z, Bagheri M, Summers RM. *ChestX-ray8: Hospital-scale Chest X-ray Database and Benchmarks on Weakly-Supervised Classification and Localization of Common Thorax Diseases.* IEEE CVPR 2017.

## Toolchain (Windows)
- MSYS2 MinGW-w64 `g++` with OpenMP
- Microsoft MPI (MS-MPI) 10.1
- OpenCV 5 (MSYS2 package, image I/O only)
- Python 3.13 venv for plotting the results (`pip install -r requirements.txt`)

## Repository layout
```
demo/            Python demo showing what each filter does to one photo
requirements.txt Python packages for analysis and plotting
```

## Status
- [x] Toolchain set up (MPI, OpenMP, OpenCV)
- [x] Dataset selected and downloaded
- [ ] Sequential baseline
- [ ] OpenMP version
- [ ] MPI version (whole-image distribution and strip decomposition with halo exchange)
- [ ] Hybrid MPI + OpenMP version
- [ ] Benchmarks: time, speedup and efficiency graphs
- [ ] Report
