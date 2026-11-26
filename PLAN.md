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

## Phase 5: Hybrid MPI + OpenMP ✅
- [x] Both MPI programs with OpenMP threads inside each process (`MPI_THREAD_FUNNELED`; MPI calls only outside parallel regions)
- [x] Configurations (processes × threads) 1×8, 2×4, 4×2, 8×1 and 2×2 on 10,000 images (`results/hybrid_phase5.csv`); **all 10 runs match the sequential checksum**

| P × T | mpi_batch | mpi_strips |
|---|---|---|
| 1×8 | **50.1 s (4.25×)** | **132.7 s (1.60×)** |
| 2×4 | 55.4 s (3.84×) | 135.2 s (1.57×) |
| 4×2 | 54.7 s (3.89×) | 145.5 s (1.46×) |
| 8×1 | 54.5 s (3.90×) | 174.8 s (1.22×) |
| 2×2 (4 workers) | 67.7 s (3.14×) | 148.1 s (1.44×) |

- Findings:
  - **Batch:** the split hardly matters on one 4-core machine (54–55 s; 1×8 at 50 s is within the ~10% run-to-run variation, which Phase 6 will check with repeats). All workers share the same memory and cores, and batch barely communicates.
  - **Strips:** fewer processes and more threads is clearly better (1×8 = 132.7 s vs 8×1 = 174.8 s). Fewer MPI processes means less communication (comm 2.9 s → 13.0 s as P grows) and fewer busy-waiting processes slowing rank 0's reading (91 s → 116 s).
  - Hybrid pays off most when communication matters. On a real cluster you would run one MPI process per machine and OpenMP threads inside each machine.

## Phase 6: Benchmarking ✅
- [x] `scripts/benchmark.ps1`: runs all 27 configurations, appends one CSV row per run, and can resume after an interruption (runs already in the CSV are skipped)
- [x] Smoke test: 50 images × 27 configurations, all with the same checksum
- [x] Final runs: **10,000 images × 27 configurations × 2 repeats = 54 runs** (about 1 h 50 min) → `results/benchmarks.csv`. The repeat loop is the outer loop, so slow drift of the machine affects every configuration equally. (This replaced the planned 1,000-image × 3 quick runs: two full passes measure consistency directly.)
- [x] **All 54 runs have checksum `f3eee2e41bf6a7ce`** (bit-identical to the sequential version)
- [x] Consistency: the two repeats differ by under 2% for 23 of the 27 configurations (worst is strips-2x1 at 7%)
- Conditions: on AC power, "Best performance" power mode, dataset folder excluded from Avast, heavy apps closed
- Final numbers. Mean of 2 runs. Speedup is against the re-measured sequential time of **195.1 s**. Efficiency = speedup / (P×T); 8 workers means 4 cores plus hyper-threads.

| Configuration | Time | Speedup | Efficiency | Read | Compute | Comm |
|---|---|---|---|---|---|---|
| sequential | 195.1 s | 1.00× | 100% | 85.5 | 103.7 | 0 |
| OpenMP rows 2 / 4 / 8 thr | 146.8 / 127.2 / 122.4 s | 1.33 / 1.53 / 1.59× | 66 / 38 / 20% | ~85 (sequential) | 55.8 / 36.7 / 31.9 | 0 |
| OpenMP images 2 / 4 / 8 thr | 103.2 / 66.6 / **55.3 s** | 1.89 / 2.93 / **3.53×** | 95 / 73 / 44% | 45.4 / 28.8 / 21.2 | 54.2 / 35.5 / 32.4 | 0 |
| OpenMP 8 thr dynamic (rows / images) | 123.0 / 55.7 s | 1.59 / 3.50× | same as static | | | |
| MPI batch 2 / 4 / 8 procs | 103.8 / 66.9 / 55.7 s | 1.88 / 2.91 / 3.50× | 94 / 73 / 44% | 45.3 / 28.5 / 21.4 | 54.9 / 36.1 / 32.7 | 0.2 |
| MPI strips 2 / 4 / 8 procs | 157.8 / 141.0 / 170.4 s | 1.24 / 1.38 / 1.14× | 62 / 35 / 14% | 86.3 / 92.8 / 113.4 | 48.3 / 27.3 / 28.7 | 12.2 / 9.2 / 12.3 |
| Hybrid batch 1×8 / 2×4 / 4×2 | 57.1 / 56.7 / 57.1 s | 3.42 / 3.44 / 3.42× | 43% | ~22 | ~33.5 | 0 |
| Hybrid strips 1×8 / 2×4 / 4×2 | 127.3 / 134.9 / 147.0 s | 1.53 / 1.45 / 1.33× | 19 / 18 / 17% | 86.6 / 91.2 / 99.6 | ~27 | 3.0 / 4.9 / 7.0 |
| 2×2 (4 workers): batch / strips | 69.8 / 145.7 s | 2.79 / 1.34× | 70 / 33% | 30.0 / 89.4 | 37.3 / 34.2 | 0 / 10.8 |

- Findings (these confirm Phases 3–5 with repeated, controlled runs):
  - **The best results are coarse-grained: whole images per worker.** OpenMP images mode (55.3 s), MPI batch 8×1 (55.7 s) and the hybrid batch splits (~57 s) are all within ~3% of each other. On one shared-memory machine, how the 8 workers are organised (threads vs processes) barely matters when they don't need to talk.
  - Phase 5's "1×8 = 50.1 s" was run-to-run noise. With repeats, every batch split lands at 56–57 s.
  - Efficiency is ~95% at 2 workers and ~73% at 4, then drops to ~44% at 8 because workers 5–8 are hyper-threads.
  - **Fine-grained versions are limited by the sequential read** (Amdahl). OpenMP rows and every strips configuration keep reading at ≥85 s on one core. Compute shrinks (103.7 → ~27 s), but the total cannot go below ~110 s.
  - **Strips gets worse with more processes:** comm grows, and busy-polling ranks slow rank 0's reading (85 → 113 s at P=8). Fewer processes with more threads (1×8) is the best strips setup.
  - Static vs dynamic scheduling: identical (uniform 1024×1024 images).

## Phase 7: Analysis and graphs ✅
- [x] `analysis/make_graphs.py` reads `results/benchmarks.csv` and writes everything below with one command (`D:\Python\python.exe analysis\make_graphs.py`)
- [x] Graphs in `report/figures/`:

| File | What it shows |
|---|---|
| `fig1_time.png` | execution time vs 1/2/4/8 workers, with the sequential time as a reference line |
| `fig2_speedup.png` | speedup vs the ideal (linear) speedup |
| `fig3_efficiency.png` | efficiency |
| `fig4_hybrid.png` | the hybrid P×T splits, batch vs strips |
| `fig5_breakdown.png` | read / compute / communication / other for 8 key configurations |
| `fig6_amdahl.png` | measured speedup vs the Amdahl prediction (f = 0.44, ceiling 2.28×) |

- [x] Summary table: `results/summary.csv` and `results/summary.md` (mean, min–max, speedup and efficiency for all 27 configurations)
- [x] Observations written in `report/observations.md`:
  - Amdahl's law (the sequential read is 44% of the time)
  - hyper-threading: efficiency 95% at 2 workers, 73% at 4, 44% at 8
  - clock speed and shared cache
  - I/O is CPU-bound PNG decoding
  - strips: communication (~40 GB moved) and busy-polling
  - hybrid improves strips by 25%
  - static and dynamic scheduling perform the same
  - why repeats matter
- Colours are validated for colour-blind readers. Each series keeps the same colour in every figure.

## Phase 8: Report (Word .docx) ✅
- [x] `report/PDC_Project_Report_Group18.docx` (22 pages, A4), plus a PDF copy for checking. Sections follow the instructor's order:
  1. Abstract
  2. Introduction
  3. Dataset Description
  4. Methodology
  5. Implementation
  6. Results
  7. Observations & Conclusion
  8. References
- [x] Cover page, table of contents with page numbers, consistent typography (Cambria body, Calibri headings), numbered figures, tables and listings
- [x] 6 code screenshots taken from the real source files with their line numbers; the lines where PDC concepts are applied are highlighted (`report/scripts/make_code_figures.py`)
- [x] Decomposition diagram, batch vs strips with halo (`report/scripts/make_diagram.py`)
- [x] A "PDC concept map" table pointing to the exact file and line of every OpenMP and MPI construct
- [x] The results table is read straight from `results/summary.csv`, so no numbers were typed by hand
- To rebuild:
  ```
  cd report\scripts
  npm install
  node build_report.js
  powershell -ExecutionPolicy Bypass -File finalize_report.ps1
  ```
  `finalize_report.ps1` uses Microsoft Word to fill in the table of contents and export the PDF.

## Phase 9: Submission and viva prep ✅
- [x] `scripts/make_submission.py` builds `../Submission_Group18/` with:
  - `1_Report/`: the report (.docx + .pdf)
  - `2_Source_Code/`: a clean `git archive` of the repository
  - `README.txt`
- [x] The dataset ZIP holds exactly the 10,000 images used, with a README (source + citation) and SHA-256 sums. At ~4 GB it is too large for Google Classroom, so it is shared on Google Drive: `--dataset-url` writes the link into README.txt, and `--with-dataset` rebuilds the ZIP if needed.
- [x] Viva guide: `viva/Project_Explained_Group18.pdf`. It explains the basics, the project, every graph and every design decision, and ends with a viva kit (one-minute summary, about 25 Q&As, numbers to remember, demo commands)

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
