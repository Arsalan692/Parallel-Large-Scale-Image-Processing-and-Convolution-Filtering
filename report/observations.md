# Observations (Phase 7, draft text for the report)

All numbers come from `results/benchmarks.csv`: 10,000 images, 27 configurations, each run twice.
The values shown are the mean of the two runs. Summary table: `results/summary.md`. Figures: `report/figures/fig1–fig6`.
The sequential time is **195.1 s** (85.5 s reading + 103.7 s filtering). All 54 runs produced the same checksum
`f3eee2e41bf6a7ce`, so every parallel version gives bit-identical output.

## 1. Coarse-grained parallelism wins (Fig. 1, 2)
- The three coarse-grained strategies each give a worker whole images, so the workers never need to exchange data. They finish within 3% of each other:

  | Strategy | Configuration | Time | Speedup |
  |---|---|---|---|
  | OpenMP images mode | 8 threads | 55.3 s | 3.53× |
  | MPI batch | 8 processes | 55.7 s | 3.50× |
  | Hybrid batch | 1×8, 2×4, 4×2 | 56.7–57.1 s | ≈3.43× |
- MPI batch communicates almost nothing:
  - one `MPI_Bcast` of the file list at the start
  - one `MPI_Reduce` of the checksums at the end
  - comm time ≈ 0.2 s out of 55.7 s
- On a single shared-memory machine, the way 8 workers are organised (threads, processes, or a mix) barely matters when they don't communicate.

## 2. Efficiency and the 4-core / 8-thread CPU (Fig. 3)
- Efficiency is **95% at 2 workers and 73% at 4**, then drops to **44% at 8**.
- The i7-8565U has **4 physical cores**. Workers 5–8 are hyper-threads that share a core's execution units with another thread, so going from 4 to 8 workers gains only ~20% (66.6 s → 55.3 s).
- Even at 4 workers, reading and filtering are each ~1.35× slower per worker than ideal (read 28.8 s vs 85.5/4 = 21.4 s; compute 35.5 s vs 25.9 s). Likely causes:
  - the clock speed drops when all cores are busy (single-core turbo is higher than all-core turbo)
  - all cores share the L3 cache and memory bandwidth
- The 2 runs of each configuration differ by under 2% for 23 of the 27 configurations, so these effects are real, not noise.

## 3. Amdahl's law: the sequential read (Fig. 5, 6)
- Reading and PNG decoding takes **44% of the sequential time** (f = 85.5 / 195.1 = 0.438).
- In **OpenMP rows mode**, only the filtering of each image is parallel. Reading stays on one thread, so Amdahl's law caps the speedup at 1/f = **2.28×** for any number of threads.
  - We measured 1.33×, 1.53× and 1.59× at 2, 4 and 8 threads, slightly below the Amdahl prediction (1.39×, 1.73×, 1.97×) because of the hyper-threading and clock effects above.
- **Images mode and MPI batch** parallelise the reading as well (each worker reads its own images). Their serial fraction is tiny, so they pass the 2.28× "ceiling" easily.
- Lesson: profile first. The obvious hotspot (the convolution) was only 53% of the time.

## 4. Strip decomposition and communication overhead (Fig. 1, 4, 5)
- In MPI strips, rank 0 reads each image and the processes work on one image at a time. Per image:

  | Step | MPI call | Data moved |
  |---|---|---|
  | send each process its rows | `MPI_Scatterv` | 1 MB |
  | exchange border rows with neighbours | 2× `MPI_Sendrecv` | 2 rows × 1,024 bytes per boundary |
  | collect the results | 3× `MPI_Gatherv` | 3 MB |
- Over 10,000 images that is ~40 GB copied between processes, with a synchronisation point every image.
- **Strips gets slower as processes are added beyond 4:** 141.0 s at P = 4 vs 170.4 s at P = 8.
  - Communication time grows (3 s at P = 1 → 12 s at P = 8).
  - MS-MPI processes busy-poll while they wait instead of sleeping. Seven waiting processes steal CPU time from rank 0, whose reading slows from 85 s to **113 s**.
- **Hybrid helps strips** (Fig. 4): with the same 8 workers, 1×8 = 127.3 s, 2×4 = 134.9 s, 4×2 = 147.0 s, 8×1 = 170.4 s. Fewer MPI processes means less communication and fewer busy-waiting processes, while OpenMP threads still share the filtering. This is the standard argument for hybrid MPI + OpenMP: one MPI process per node, threads inside the node.
- Strip decomposition is the right tool for **one image too large for a single process**. For **many small, independent images**, distributing whole images (batch) is far better: 55.7 s vs 127.3 s for the best strips setup.

## 5. Scheduling (static vs dynamic)
- `schedule(static)` and `schedule(dynamic)` give the same time within 1%:
  - rows mode: 122.4 s vs 123.0 s
  - images mode: 55.3 s vs 55.7 s
- Every image is 1024×1024 and every row costs the same, so the load is already balanced. Dynamic scheduling would only help with uneven work, such as images of different sizes.

## 6. I/O
- Reading is CPU-bound PNG decompression, not disk-bound. It speeds up almost linearly with parallel readers (85.5 → 45.4 → 28.8 s for 1 → 2 → 4), which would not happen if the disk were the bottleneck.
- Only a few output images are saved (`--save`), so writing to disk does not distort the timings.

## 7. Measurement methodology
- Every configuration is run twice. The repeat loop is the outer loop, so slow drift of the machine (temperature, background tasks) affects all configurations equally.
- Without repeats we would have drawn wrong conclusions:
  - An early sequential run with background load took 559 s instead of ~200 s.
  - Phase 5's single-run result of 50.1 s for hybrid batch 1×8 was noise; with repeats it is 57.1 s.
- Speedups are computed against a sequential run measured in the same session (195.1 s), not the earlier 212.8 s baseline.

## Conclusion (short)
- On a 4-core laptop the best result is **55 s instead of 195 s (3.5×)** for 10,000 X-rays × 3 filters, with bit-identical output.
- The winning design is coarse-grained: whole images per worker, with the reading parallelised as well.
- Fine-grained decomposition (rows or strips) is limited by Amdahl's law and by communication.
- Hybrid MPI + OpenMP matters most when communication matters: it improves strips by 25% over pure MPI (127.3 s vs 170.4 s).
