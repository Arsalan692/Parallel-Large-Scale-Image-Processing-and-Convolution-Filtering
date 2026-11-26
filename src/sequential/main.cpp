// sequential/main.cpp
// PHASE 2 - Sequential baseline.
// One core processes every image, one row after another. Every parallel version
// is compared against the time of this program (speedup = T_sequential / T_parallel).
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
        const double t_start = wall_time();

        const std::vector<std::string> files = list_images(opt.input_dir, opt.count);
        if (files.empty()) throw std::runtime_error("no .png images found in " + opt.input_dir);
        if (static_cast<int>(files.size()) < opt.count)
            std::printf("Warning: only %zu images available (asked for %d)\n", files.size(), opt.count);
        std::printf("Sequential: processing %zu images from %s\n", files.size(), opt.input_dir.c_str());

        const std::string save_dir = (std::filesystem::path(opt.output_dir) / "sequential").string();
        PhaseTimes times;
        uint64_t checksum = 0;

        for (size_t i = 0; i < files.size(); ++i) {
            // 1. Read the image from disk.
            double t0 = wall_time();
            const GrayImage img = load_gray(files[i]);
            double t1 = wall_time();
            times.read += t1 - t0;

            // 2. Apply the three filters to every row (single core, no parallelism).
            FilterOutputs out(img.width, img.height);
            const ImageView view = whole_image_view(img);
            for (int y = 0; y < img.height; ++y)
                filter_row(view, y, out.blur.row(y), out.sharpen.row(y), out.edges.row(y));
            double t2 = wall_time();
            times.compute += t2 - t1;

            // 3. Add this image's fingerprint to the dataset checksum.
            checksum = combine_checksums(checksum, outputs_checksum(out));

            // 4. Save the outputs of the first few images (samples for the report).
            if (static_cast<int>(i) < opt.save) {
                const double t3 = wall_time();
                save_outputs(save_dir, files[i], out);
                times.write += wall_time() - t3;
            }

            if ((i + 1) % 1000 == 0)
                std::fprintf(stderr, "  ... %zu / %zu images done (%.0f s)\n", i + 1, files.size(),
                             wall_time() - t_start);
        }

        RunResult result;
        result.version = "sequential";
        result.procs = 1;
        result.threads = 1;
        result.images = static_cast<int>(files.size());
        result.times = times;
        result.total = wall_time() - t_start;
        result.checksum = checksum;
        result.label = opt.label;

        print_result(result);
        if (!opt.csv.empty()) append_csv(opt.csv, result);
        if (opt.save > 0) std::printf("  samples  saved to %s\n", save_dir.c_str());
    } catch (const std::exception& e) {
        std::fprintf(stderr, "Error: %s\n", e.what());
        return 1;
    }
    return 0;
}
