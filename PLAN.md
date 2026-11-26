# Project Plan

**Hybrid Parallel Large-Scale Image Processing and Convolution Filtering**
PDC (CS3006), Group 18, Project #6, Hybrid MPI + OpenMP

## Goal
Apply three convolution filters (Gaussian blur 5×5, sharpen 3×3, Sobel edge detection) to 10,000 chest X-rays (NIH ChestX-ray14, 1024×1024 grayscale). Measure execution time, speedup and efficiency for four setups:
- sequential
- OpenMP
- MPI
- hybrid MPI + OpenMP

## Ground rules (apply to every phase)
1. **One convolution function shared by every version.** The only thing that changes between versions is how the work is split, so the comparison is fair.
2. **Bit-identical output.** Every parallel version must produce exactly the same pixels as the sequential version. We check this with a checksum of all outputs.
3. **Separate timings.** We measure read (I/O), compute and communication separately.
4. **Few saved images.** Only a sample of outputs is written to disk, so file writing doesn't distort the timings.
5. **MPI-only = hybrid with 1 thread.** The MPI programs accept a thread count, so the same program covers both MPI-only and hybrid runs.

---

## Phase 0: Setup ✅
- [x] Toolchain: MSYS2 g++ 16.2 + OpenMP, MS-MPI 10.1, OpenCV 5
- [x] Dataset: 14,999 images in `E:\X-Ray Dataset\images` (archives verified with SHA-256)
- [x] Python venv (`venv/`), GitHub repo

## Phase 1: Project skeleton and shared code ✅
- [x] Folder structure (`src/`, `scripts/`, `tests/`, `results/`, `analysis/`, `report/`)
- [x] Shared code: image I/O, kernels, convolution, timer, checksum, command-line options, CSV results
- [x] `scripts/build.ps1` (one-command build) and `scripts/run.ps1` (runs with the correct DLL path, with or without `mpiexec`)
- [x] Unit tests for the shared code (`tests/test_common.cpp`): 9/9 pass
- **Done when:** everything builds with one command and all tests pass

## Phase 2: Sequential baseline ✅
- [x] `src/sequential/main.cpp`: one core, every image, all 3 filters
- [x] Validate our convolution against OpenCV `filter2D` / `Sobel` (`tests/validate_opencv.cpp`, 50 X-rays): sharpen and Sobel are 100% identical; blur is 99.82% identical, and the other 0.18% are exact x.5 rounding ties (we round .5 up, OpenCV rounds .5 to even), with 0 real mismatches
- [x] Sample outputs + `report/figures/sample_filters.png`
- [x] **Baseline (10,000 images): 212.8 s total = 93.6 s read + 111.8 s compute (+ checksum/overhead)**; checksum `f3eee2e41bf6a7ce` (`results/sequential_baseline.csv`)
- Lessons:
  - Reading (PNG decoding) costs about as much as filtering, so the parallel versions should also parallelise reading.
  - Background load can double run times. Benchmarks need a quiet machine (an earlier run was 559 s with other activity running).

## Phase 3: OpenMP version ✅
- [x] `src/openmp/main.cpp` with two work-sharing modes:
  - `--mode rows`: `#pragma omp parallel for` over the rows of each image (fine-grained; reading stays sequential)
  - `--mode images`: `#pragma omp parallel for reduction(+: …)` over images (coarse-grained; each thread reads and filters whole images)
- [x] 1, 2, 4 and 8 threads on 10,000 images (`results/openmp_phase3.csv`); **all 12 runs match the sequential checksum `f3eee2e41bf6a7ce`**
- [x] `schedule(static)` vs `schedule(dynamic)`: no difference (all images are the same size, so the load is already balanced)
- Results (speedup vs the 212.8 s baseline):

| Threads | rows mode | images mode |
|---|---|---|
| 1 | 203.2 s (1.05×) | 203.1 s (1.05×) |
| 2 | 153.4 s (1.39×) | 103.4 s (2.06×) |
| 4 | 134.3 s (1.58×) | 66.7 s (3.19×) |
| 8 | 129.1 s (1.65×) | **56.9 s (3.74×)** |

- Findings:
  - In rows mode, reading (~90 s) stays sequential. By Amdahl's law the maximum speedup is about 2.3×, so we only reach 1.65×.
  - Going from 4 to 8 threads gains only about 15%: the laptop has 4 physical cores, and threads 5–8 are hyper-threads sharing those cores.
  - 1-thread runs were about 5% faster than the baseline (run-to-run variation). In Phase 6 every configuration, including sequential, is re-run under the same conditions.

## Phase 4: MPI versions (the main contribution) ✅
- [x] **4A. Batch distribution** (`src/mpi_batch/`): `MPI_Bcast` of the file list, block partitioning of the images, `MPI_Reduce` of the checksums, `MPI_Gather` of the per-process timings
- [x] **4B. Strip decomposition** (`src/mpi_strips/`), for every image:
  - `MPI_Bcast` the image size
  - `MPI_Scatterv` the strips
  - 2× `MPI_Sendrecv` halo exchange (2 rows each way, `MPI_PROC_NULL` at the ends)
  - 3× `MPI_Gatherv` to collect the outputs
- [x] 1, 2, 4 and 8 processes on 10,000 images (`results/mpi_phase4.csv`, per-process tables in `results/raw/phase4_log.txt`); **all 8 runs match the sequential checksum**

| Processes | mpi_batch | mpi_strips |
|---|---|---|
| 1 | 203.8 s (1.04×) | 207.8 s (1.02×) |
| 2 | 103.8 s (2.05×) | 157.4 s (1.35×) |
| 4 | 65.8 s (3.24×) | 141.3 s (1.51×) |
| 8 | **54.9 s (3.88×)** | 170.9 s (1.25×, slower than 4) |

- Findings:
  - **Batch** is almost perfectly balanced (1,250 images and about 32 s compute per process at P=8) and has almost no communication. It is the best result so far.
  - **Strips:** only rank 0 reads (about 92 s, sequential), so the other ranks spend most of their time waiting inside MPI calls (about 110 s at P=4). Amdahl's law again.
  - **Strips at P=8 is slower than at P=4.** 8 processes share 4 cores, and MPI processes that are waiting keep polling (busy-waiting) instead of sleeping. They steal CPU time from rank 0, whose reading slows from 92 s to 114 s.
  - Strip decomposition is designed for **one very large image** that one process cannot handle. For a batch of many small images, distributing whole images (batch) is far better.

## Phase 5: Hybrid MPI + OpenMP
- [ ] Both MPI programs with OpenMP threads inside each process (`MPI_THREAD_FUNNELED`)
- [ ] Configurations (processes × threads): 1×8, 2×4, 4×2, 8×1, and 2×2
- **Key question:** with the same total of 8 workers, which split is fastest, and why?

## Phase 6: Benchmarking
- [ ] An automated script runs every configuration and writes the results to a CSV file
- [ ] Quick runs: 1,000 images, repeated 3× (to check consistency)
- [ ] Final runs: 10,000 images
- Conditions: on AC power, "Best performance" power plan, dataset folder excluded from Avast (done), heavy apps closed

## Phase 7: Analysis and graphs
- [ ] Graphs:
  - execution time
  - speedup
  - efficiency
  - batch vs strips
  - the hybrid configurations
  - compute / communication / I/O breakdown
- [ ] Observations:
  - Amdahl's law
  - 4 physical cores vs 8 logical cores (hyper-threading)
  - turbo boost and thermal limits
  - I/O cost
  - communication overhead of strip decomposition

## Phase 8: Report (Word .docx)
Sections in the instructor's order:
1. Abstract
2. Introduction
3. Dataset Description
4. Methodology
5. Implementation (code screenshots showing where each PDC concept is applied)
6. Results
7. Observations & Conclusion

## Phase 9: Submission and viva prep
- [ ] ZIP of the 10,000 images used, plus source code and the report
- [ ] Viva Q&A sheet

---

## Experiment matrix
| ID | Version | Configurations | Images |
|---|---|---|---|
| E1 | Sequential | 1 | 10,000 |
| E2 | OpenMP | 1, 2, 4, 8 threads; static vs dynamic | 10,000 |
| E3 | MPI batch | 1, 2, 4, 8 processes | 10,000 |
| E4 | MPI strips | 1, 2, 4, 8 processes | 10,000 |
| E5 | Hybrid | 1×8, 2×4, 4×2, 8×1, 2×2 (batch and strips) | 10,000 |
| E6 | Correctness | checksum of every run equals the sequential checksum | all |

## Hardware
Intel Core i7-8565U (4 cores / 8 threads, 1.8 GHz base, turbo up to 4.6 GHz), 16 GB RAM, Windows 11.
