// mpi_batch/main.cpp
// PHASE 4A (MPI) and PHASE 5 (hybrid) - batch distribution of whole images.
//
// The N images are split into P contiguous blocks, one block per MPI process
// (block partitioning, see common/partition.hpp). Every process reads, filters and
// fingerprints ITS OWN images independently, so communication is small:
//   start : MPI_Bcast  - rank 0 sends the list of image files to all processes
//   end   : MPI_Reduce - the per-process checksums are summed on rank 0
//           MPI_Gather - per-process timings are collected for the report
//
// Hybrid (--threads T > 1): inside each process, T OpenMP threads share that
// process's images. MPI calls are made only outside parallel regions, by the main
// thread, so MPI_THREAD_FUNNELED is sufficient.
//
// Run: mpiexec -n <P> mpi_batch.exe --count 10000 [--threads T]
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

    // MPI-only by default (1 thread per process); --threads T gives the hybrid version.
    const int threads = opt.threads > 0 ? opt.threads : 1;
    omp_set_num_threads(threads);
    omp_set_schedule(opt.schedule == "dynamic" ? omp_sched_dynamic : omp_sched_static,
                     opt.schedule == "dynamic" ? 1 : 0);

    MPI_Barrier(MPI_COMM_WORLD);  // all processes start the clock together
    const double t_start = MPI_Wtime();
    double comm_time = 0.0;

    // ---- 1. Rank 0 lists the images and broadcasts the list ---------------------------
    std::vector<std::string> root_files;
    int ok = 1;
    if (rank == 0) {
        try {
            root_files = list_images(opt.input_dir, opt.count);
            if (root_files.empty()) throw std::runtime_error("no .png images in " + opt.input_dir);
        } catch (const std::exception& e) {
            std::fprintf(stderr, "Error: %s\n", e.what());
            ok = 0;
        }
    }
    double c0 = MPI_Wtime();
    MPI_Bcast(&ok, 1, MPI_INT, 0, MPI_COMM_WORLD);
    if (!ok) {
        MPI_Finalize();
        return 1;
    }
    const std::vector<std::string> files = broadcast_file_list(root_files, rank, MPI_COMM_WORLD);
    comm_time += MPI_Wtime() - c0;

    const int n = static_cast<int>(files.size());
    if (rank == 0)
        std::printf("MPI batch: %d images, %d processes x %d threads\n", n, size, threads);

    // ---- 2. Block partitioning: which images belong to this process ------------------
    const Range mine = split_range(n, size, rank);

    const std::string save_dir = (std::filesystem::path(opt.output_dir) / "mpi_batch").string();
    if (opt.save > 0) std::filesystem::create_directories(save_dir);

    // ---- 3. Each process filters its own images (OpenMP threads share them) -----------
    uint64_t local_checksum = 0;
    double read_sum = 0, compute_sum = 0, write_sum = 0;
    std::string failure;

    #pragma omp parallel for schedule(runtime) reduction(+ : local_checksum, read_sum, compute_sum, write_sum)
    for (int i = mine.begin; i < mine.end; ++i) {
        try {
            const double t0 = wall_time();
            const GrayImage img = load_gray(files[i]);
            const double t1 = wall_time();

            FilterOutputs out(img.width, img.height);
            const ImageView view = whole_image_view(img);
            for (int y = 0; y < img.height; ++y)
                filter_row(view, y, out.blur.row(y), out.sharpen.row(y), out.edges.row(y));
            const double t2 = wall_time();

            read_sum += t1 - t0;
            compute_sum += t2 - t1;
            local_checksum += outputs_checksum(out);

            if (i < opt.save) {
                const double t3 = wall_time();
                save_outputs(save_dir, files[i], out);
                write_sum += wall_time() - t3;
            }
        } catch (const std::exception& e) {
            #pragma omp critical
            failure = e.what();
        }
    }
    if (!failure.empty()) {
        std::fprintf(stderr, "Rank %d error: %s\n", rank, failure.c_str());
        MPI_Abort(MPI_COMM_WORLD, 1);
    }

    // ---- 4. Combine the per-process checksums on rank 0 -------------------------------
    // Rank 0's time in MPI_Reduce also includes waiting for the slowest process.
    const double c1 = MPI_Wtime();
    uint64_t checksum = 0;
    MPI_Reduce(&local_checksum, &checksum, 1, MPI_UNSIGNED_LONG_LONG, MPI_SUM, 0, MPI_COMM_WORLD);
    comm_time += MPI_Wtime() - c1;
    const double total = MPI_Wtime() - t_start;

    // ---- 5. Report (not part of the timed work) ---------------------------------------
    const RankStats stats{static_cast<double>(mine.size()), read_sum / threads, compute_sum / threads, comm_time};
    print_rank_table(stats, rank, size, MPI_COMM_WORLD, "images");

    if (rank == 0) {
        RunResult result;
        result.version = "mpi_batch";
        result.mode = "images";
        result.schedule = threads > 1 ? opt.schedule : "";
        result.procs = size;
        result.threads = threads;
        result.images = n;
        result.times = PhaseTimes{stats.read, stats.compute, comm_time, write_sum / threads};  // as seen by rank 0
        result.total = total;
        result.checksum = checksum;
        result.label = opt.label;
        print_result(result);
        if (!opt.csv.empty()) append_csv(opt.csv, result);
    }

    MPI_Finalize();
    return 0;
}
