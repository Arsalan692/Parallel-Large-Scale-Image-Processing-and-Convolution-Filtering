// openmp/main.cpp
// PHASE 3 - OpenMP version (shared-memory threads on one machine).
//
// Two ways of sharing the work among the threads (--mode):
//   rows   : images are handled one after another; the ROWS of each image are divided
//            among the threads (fine-grained). Reading each image stays sequential.
//   images : the IMAGES are divided among the threads; each thread reads and filters
//            whole images on its own (coarse-grained). Reading is parallel too.
//
// Loop scheduling (--schedule):
//   static  : the loop is cut into equal contiguous blocks up front, one block per thread
//   dynamic : threads grab the next chunk when they finish one (better if work is uneven)
// The pragmas use schedule(runtime), and omp_set_schedule() picks static or dynamic.
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
#include "common/options.hpp"
#include "common/results.hpp"
#include "common/timer.hpp"

using namespace pdc;

// ---------------------------------------------------------------------------
// Mode "rows": threads share the rows of one image at a time.
// ---------------------------------------------------------------------------
static uint64_t run_rows_mode(const std::vector<std::string>& files, const Options& opt,
                              const std::string& save_dir, PhaseTimes& times, double t_start) {
    uint64_t checksum = 0;
    for (size_t i = 0; i < files.size(); ++i) {
        const double t0 = wall_time();
        const GrayImage img = load_gray(files[i]);  // read by one thread (sequential part)
        const double t1 = wall_time();

        FilterOutputs out(img.width, img.height);
        const ImageView view = whole_image_view(img);

        // PDC: the 1024 rows of this image are divided among the OpenMP threads.
        // Each row is independent (it only READS the shared input and WRITES its own
        // output row), so no locks are needed.
        #pragma omp parallel for schedule(runtime)
        for (int y = 0; y < img.height; ++y)
            filter_row(view, y, out.blur.row(y), out.sharpen.row(y), out.edges.row(y));
        // (implicit barrier: all threads finish this image before the next one starts)

        const double t2 = wall_time();
        times.read += t1 - t0;
        times.compute += t2 - t1;
        checksum = combine_checksums(checksum, outputs_checksum(out));

        if (static_cast<int>(i) < opt.save) {
            const double t3 = wall_time();
            save_outputs(save_dir, files[i], out);
            times.write += wall_time() - t3;
        }
        if ((i + 1) % 1000 == 0)
            std::fprintf(stderr, "  ... %zu / %zu images done (%.0f s)\n", i + 1, files.size(),
                         wall_time() - t_start);
    }
    return checksum;
}

// ---------------------------------------------------------------------------
// Mode "images": threads share the list of images; each thread does whole images.
// ---------------------------------------------------------------------------
static uint64_t run_images_mode(const std::vector<std::string>& files, const Options& opt,
                                const std::string& save_dir, PhaseTimes& times, double t_start,
                                int threads) {
    const int n = static_cast<int>(files.size());
    uint64_t checksum = 0;
    double read_sum = 0, compute_sum = 0, write_sum = 0;  // busy time summed over all threads
    int done = 0;
    std::string failure;

    // PDC: the images are divided among the threads. Every thread reads, filters and
    // fingerprints its own images. reduction(+: ...) gives each thread a private copy of
    // these variables and adds the copies together at the end (no race conditions).
    #pragma omp parallel for schedule(runtime) reduction(+ : checksum, read_sum, compute_sum, write_sum)
    for (int i = 0; i < n; ++i) {
        try {
            const double t0 = wall_time();
            const GrayImage img = load_gray(files[i]);  // reading is parallel in this mode
            const double t1 = wall_time();

            FilterOutputs out(img.width, img.height);
            const ImageView view = whole_image_view(img);
            for (int y = 0; y < img.height; ++y)
                filter_row(view, y, out.blur.row(y), out.sharpen.row(y), out.edges.row(y));
            const double t2 = wall_time();

            read_sum += t1 - t0;
            compute_sum += t2 - t1;
            checksum += outputs_checksum(out);  // combine_checksums is "+", done by the reduction

            if (i < opt.save) {
                const double t3 = wall_time();
                save_outputs(save_dir, files[i], out);
                write_sum += wall_time() - t3;
            }
        } catch (const std::exception& e) {
            // Exceptions must not escape an OpenMP loop: record the error instead.
            #pragma omp critical
            failure = e.what();
        }

        int finished;
        #pragma omp atomic capture
        finished = ++done;  // shared progress counter, updated atomically
        if (finished % 1000 == 0)
            std::fprintf(stderr, "  ... %d / %d images done (%.0f s)\n", finished, n, wall_time() - t_start);
    }
    if (!failure.empty()) throw std::runtime_error(failure);

    // Threads worked at the same time, so report the AVERAGE busy time per thread.
    times.read = read_sum / threads;
    times.compute = compute_sum / threads;
    times.write = write_sum / threads;
    return checksum;
}

int main(int argc, char** argv) {
    Options opt;
    std::string error;
    if (!parse_options(argc, argv, opt, error)) {
        std::fprintf(stderr, "Error: %s\n\n", error.c_str());
        print_usage(argv[0]);
        return 1;
    }
    if (opt.help) {
        print_usage(argv[0]);
        return 0;
    }

    try {
        // Number of threads and loop schedule.
        if (opt.threads > 0) omp_set_num_threads(opt.threads);
        const int threads = omp_get_max_threads();
        const bool dynamic = opt.schedule == "dynamic";
        const int chunk = (opt.mode == "rows") ? 16 : 1;  // dynamic: 16 rows / 1 image per grab
        omp_set_schedule(dynamic ? omp_sched_dynamic : omp_sched_static, dynamic ? chunk : 0);

        const double t_start = wall_time();
        const std::vector<std::string> files = list_images(opt.input_dir, opt.count);
        if (files.empty()) throw std::runtime_error("no .png images found in " + opt.input_dir);
        std::printf("OpenMP: %zu images, %d threads, mode=%s, schedule=%s\n", files.size(), threads,
                    opt.mode.c_str(), opt.schedule.c_str());

        const std::string save_dir = (std::filesystem::path(opt.output_dir) / "openmp").string();
        if (opt.save > 0) std::filesystem::create_directories(save_dir);

        PhaseTimes times;
        const uint64_t checksum = (opt.mode == "rows")
                                      ? run_rows_mode(files, opt, save_dir, times, t_start)
                                      : run_images_mode(files, opt, save_dir, times, t_start, threads);

        RunResult result;
        result.version = "openmp";
        result.mode = opt.mode;
        result.schedule = opt.schedule;
        result.procs = 1;
        result.threads = threads;
        result.images = static_cast<int>(files.size());
        result.times = times;
        result.total = wall_time() - t_start;
        result.checksum = checksum;
        result.label = opt.label;

        print_result(result);
        if (!opt.csv.empty()) append_csv(opt.csv, result);
    } catch (const std::exception& e) {
        std::fprintf(stderr, "Error: %s\n", e.what());
        return 1;
    }
    return 0;
}
