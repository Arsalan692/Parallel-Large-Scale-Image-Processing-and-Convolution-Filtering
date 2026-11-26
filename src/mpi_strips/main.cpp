// mpi_strips/main.cpp
// PHASE 4B (MPI) and PHASE 5 (hybrid) - strip decomposition with halo exchange.
//
// Every image is split into P horizontal strips, one per MPI process (domain decomposition):
//   1. rank 0 reads the image from disk
//   2. MPI_Bcast     image width/height to all processes
//   3. MPI_Scatterv  each process receives ITS OWN rows of the image
//   4. MPI_Sendrecv  HALO EXCHANGE: neighbouring processes swap kMaxRadius (= 2) border
//                    rows, because the 5x5 blur at a strip's edge needs pixels that
//                    belong to the neighbouring strip
//   5. each process filters its strip (with --threads T > 1, OpenMP threads share the rows)
//   6. MPI_Gatherv   rank 0 collects the filtered strips into the full output images
//
//   Strip buffer of a middle process:
//     [ 2 halo rows from the process above ]
//     [ own rows (received with Scatterv)  ]
//     [ 2 halo rows from the process below ]
//
// Run: mpiexec -n <P> mpi_strips.exe --count 10000 [--threads T]
#include <mpi.h>
#include <omp.h>

#include <cstdio>
#include <exception>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

#include "common/checksum.hpp"
#include "common/convolution.hpp"
#include "common/image_io.hpp"
#include "common/kernels.hpp"
#include "common/mpi_utils.hpp"
#include "common/options.hpp"
#include "common/partition.hpp"
#include "common/results.hpp"
#include "common/timer.hpp"

using namespace pdc;

int main(int argc, char** argv) {
    int provided = 0;
    MPI_Init_thread(&argc, &argv, MPI_THREAD_FUNNELED, &provided);
    int rank = 0, size = 1;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    Options opt;
    std::string error;
    if (!parse_options(argc, argv, opt, error) || opt.help) {
        if (rank == 0) {
            if (!error.empty()) std::fprintf(stderr, "Error: %s\n\n", error.c_str());
            print_usage(argv[0]);
        }
        MPI_Finalize();
        return opt.help ? 0 : 1;
    }

    const int threads = opt.threads > 0 ? opt.threads : 1;
    omp_set_num_threads(threads);
    omp_set_schedule(opt.schedule == "dynamic" ? omp_sched_dynamic : omp_sched_static,
                     opt.schedule == "dynamic" ? 16 : 0);

    MPI_Barrier(MPI_COMM_WORLD);
    const double t_start = MPI_Wtime();
    PhaseTimes times;  // this process's own timings

    // ---- Rank 0 lists the images, everyone learns how many there are ------------------
    std::vector<std::string> files;
    int n = 0;
    if (rank == 0) {
        try {
            files = list_images(opt.input_dir, opt.count);
        } catch (const std::exception& e) {
            std::fprintf(stderr, "Error: %s\n", e.what());
        }
        n = static_cast<int>(files.size());
        std::printf("MPI strips: %d images, %d processes x %d threads\n", n, size, threads);
    }
    MPI_Bcast(&n, 1, MPI_INT, 0, MPI_COMM_WORLD);
    if (n == 0) {
        MPI_Finalize();
        return 1;
    }

    const std::string save_dir = (std::filesystem::path(opt.output_dir) / "mpi_strips").string();
    if (rank == 0 && opt.save > 0) std::filesystem::create_directories(save_dir);

    const int up = rank > 0 ? rank - 1 : MPI_PROC_NULL;           // neighbour above
    const int down = rank < size - 1 ? rank + 1 : MPI_PROC_NULL;  // neighbour below
    const int R = kMaxRadius;                                      // halo rows per side

    uint64_t checksum = 0;
    std::vector<int> counts(size), displs(size);
    std::vector<uint8_t> strip, blur, sharpen, edges;
    int rows_per_image = 0;

    for (int i = 0; i < n; ++i) {
        // ---- 1. Rank 0 reads the image ----------------------------------------------
        GrayImage img;
        int dims[2] = {0, 0};
        if (rank == 0) {
            const double t0 = wall_time();
            try {
                img = load_gray(files[i]);
            } catch (const std::exception& e) {
                std::fprintf(stderr, "Error: %s\n", e.what());
                MPI_Abort(MPI_COMM_WORLD, 1);  // stop every process, not just rank 0
            }
            times.read += wall_time() - t0;
            dims[0] = img.width;
            dims[1] = img.height;
        }

        // ---- 2. Everyone learns the image size --------------------------------------
        double c = MPI_Wtime();
        MPI_Bcast(dims, 2, MPI_INT, 0, MPI_COMM_WORLD);
        times.comm += MPI_Wtime() - c;
        const int W = dims[0], H = dims[1];
        if (H < size * R) {
            if (rank == 0) std::fprintf(stderr, "Error: image too small for %d strips\n", size);
            MPI_Abort(MPI_COMM_WORLD, 1);
        }

        // Which rows belong to which process (same block partitioning on every process).
        for (int r = 0; r < size; ++r) {
            const Range rr = split_range(H, size, r);
            counts[r] = rr.size() * W;
            displs[r] = rr.begin * W;
        }
        const Range own = split_range(H, size, rank);
        const int halo_top = (up != MPI_PROC_NULL) ? R : 0;
        const int halo_bottom = (down != MPI_PROC_NULL) ? R : 0;
        rows_per_image = own.size();

        strip.resize(static_cast<size_t>(halo_top + own.size() + halo_bottom) * W);
        uint8_t* own_rows = strip.data() + static_cast<size_t>(halo_top) * W;
        const int halo_bytes = R * W;

        c = MPI_Wtime();
        // ---- 3. Scatter: each process receives its own rows ------------------------
        MPI_Scatterv(rank == 0 ? img.pixels.data() : nullptr, counts.data(), displs.data(),
                     MPI_UNSIGNED_CHAR, own_rows, own.size() * W, MPI_UNSIGNED_CHAR, 0, MPI_COMM_WORLD);

        // ---- 4. Halo exchange with the neighbours ---------------------------------
        // (a) send my first R rows UP, receive my bottom halo from the process DOWN
        MPI_Sendrecv(own_rows, halo_bytes, MPI_UNSIGNED_CHAR, up, 0,
                     own_rows + static_cast<size_t>(own.size()) * W, halo_bytes, MPI_UNSIGNED_CHAR, down, 0,
                     MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        // (b) send my last R rows DOWN, receive my top halo from the process UP
        MPI_Sendrecv(own_rows + static_cast<size_t>(own.size() - R) * W, halo_bytes, MPI_UNSIGNED_CHAR, down, 1,
                     strip.data(), halo_bytes, MPI_UNSIGNED_CHAR, up, 1, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        times.comm += MPI_Wtime() - c;

        // ---- 5. Filter my own rows (OpenMP threads share them in the hybrid version) --
        const double t1 = wall_time();
        const ImageView view{strip.data(), W, H, own.begin - halo_top, halo_top + own.size() + halo_bottom};
        const size_t own_bytes = static_cast<size_t>(own.size()) * W;
        blur.resize(own_bytes);
        sharpen.resize(own_bytes);
        edges.resize(own_bytes);

        #pragma omp parallel for schedule(runtime)
        for (int y = own.begin; y < own.end; ++y) {
            const size_t off = static_cast<size_t>(y - own.begin) * W;
            filter_row(view, y, blur.data() + off, sharpen.data() + off, edges.data() + off);
        }
        times.compute += wall_time() - t1;

        // ---- 6. Gather the filtered strips on rank 0 -------------------------------
        FilterOutputs out;
        if (rank == 0) out = FilterOutputs(W, H);
        c = MPI_Wtime();
        MPI_Gatherv(blur.data(), own.size() * W, MPI_UNSIGNED_CHAR, rank == 0 ? out.blur.pixels.data() : nullptr,
                    counts.data(), displs.data(), MPI_UNSIGNED_CHAR, 0, MPI_COMM_WORLD);
        MPI_Gatherv(sharpen.data(), own.size() * W, MPI_UNSIGNED_CHAR,
                    rank == 0 ? out.sharpen.pixels.data() : nullptr, counts.data(), displs.data(),
                    MPI_UNSIGNED_CHAR, 0, MPI_COMM_WORLD);
        MPI_Gatherv(edges.data(), own.size() * W, MPI_UNSIGNED_CHAR, rank == 0 ? out.edges.pixels.data() : nullptr,
                    counts.data(), displs.data(), MPI_UNSIGNED_CHAR, 0, MPI_COMM_WORLD);
        times.comm += MPI_Wtime() - c;

        // ---- Rank 0 fingerprints (and optionally saves) the complete outputs ----------
        if (rank == 0) {
            checksum = combine_checksums(checksum, outputs_checksum(out));
            if (i < opt.save) {
                const double t3 = wall_time();
                save_outputs(save_dir, files[i], out);
                times.write += wall_time() - t3;
            }
            if ((i + 1) % 1000 == 0)
                std::fprintf(stderr, "  ... %d / %d images done (%.0f s)\n", i + 1, n, MPI_Wtime() - t_start);
        }
    }

    MPI_Barrier(MPI_COMM_WORLD);
    const double total = MPI_Wtime() - t_start;

    // ---- Report (not part of the timed work) ----------------------------------------
    const RankStats stats{static_cast<double>(rows_per_image), times.read, times.compute, times.comm};
    print_rank_table(stats, rank, size, MPI_COMM_WORLD, "rows/img");

    if (rank == 0) {
        RunResult result;
        result.version = "mpi_strips";
        result.mode = "rows";
        result.schedule = threads > 1 ? opt.schedule : "";
        result.procs = size;
        result.threads = threads;
        result.images = n;
        result.times = times;  // as seen by rank 0
        result.total = total;
        result.checksum = checksum;
        result.label = opt.label;
        print_result(result);
        if (!opt.csv.empty()) append_csv(opt.csv, result);
    }

    MPI_Finalize();
    return 0;
}
