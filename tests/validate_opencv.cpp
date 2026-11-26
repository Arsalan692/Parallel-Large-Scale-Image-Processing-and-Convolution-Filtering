// validate_opencv.cpp
// Checks our hand-written filters against OpenCV's own implementations on real X-rays.
// OpenCV is used here ONLY as an independent reference; it is never used in timed runs.
//
//   ours: Gaussian 5x5   vs  cv::filter2D (same kernel, BORDER_REPLICATE)
//   ours: Sharpen 3x3    vs  cv::filter2D (same kernel, BORDER_REPLICATE)
//   ours: Sobel edges    vs  cv::Sobel (x and y) + cv::magnitude
//
// OpenCV computes each result as an exact real value (float) and then rounds it.
// Rounding conventions differ: we round x.5 UP, OpenCV rounds x.5 to the nearest EVEN
// number. So a pixel whose exact value is x.5 may legitimately differ by 1.
// The validator classifies every pixel:
//   identical          - same 8-bit value
//   rounding tie       - differs by 1 AND OpenCV's exact value is x.5 (convention only)
//   real mismatch      - anything else  -> validation FAILS
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <string>
#include <vector>

#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include "common/convolution.hpp"
#include "common/image_io.hpp"
#include "common/kernels.hpp"
#include "common/options.hpp"

using namespace pdc;

struct Stats {
    long long pixels = 0, identical = 0, ties = 0, mismatches = 0;
};

// exact: OpenCV's result before rounding (CV_32F). Its rounded 8-bit value is the reference.
static void compare(const GrayImage& ours, const cv::Mat& exact, Stats& s) {
    for (int y = 0; y < exact.rows; ++y)
        for (int x = 0; x < exact.cols; ++x) {
            const float v = exact.at<float>(y, x);
            const int ref = cv::saturate_cast<uint8_t>(v);  // OpenCV's own rounding
            const int mine = ours.row(y)[x];
            s.pixels++;
            if (mine == ref) {
                s.identical++;
            } else if (std::abs(mine - ref) == 1 && v > 0.0f && v < 255.0f &&
                       std::fabs(v - std::floor(v) - 0.5f) < 1e-4f) {
                s.ties++;  // exact value is x.5: only the rounding convention differs
            } else {
                s.mismatches++;
            }
        }
}

static cv::Mat kernel_mat(const Kernel& k) {
    cv::Mat m(k.size, k.size, CV_32F);
    for (int i = 0; i < k.size; ++i)
        for (int j = 0; j < k.size; ++j)
            m.at<float>(i, j) = static_cast<float>(k.weights[i * k.size + j]) / k.divisor;
    return m;
}

int main(int argc, char** argv) {
    Options opt;
    opt.count = 20;  // default for validation: 20 images
    std::string error;
    if (!parse_options(argc, argv, opt, error) || opt.help) {
        if (!error.empty()) std::fprintf(stderr, "Error: %s\n", error.c_str());
        print_usage(argv[0]);
        return opt.help ? 0 : 1;
    }

    try {
        const auto files = list_images(opt.input_dir, opt.count);
        std::printf("Validating our filters against OpenCV on %zu real X-ray images...\n\n", files.size());

        const cv::Mat k_blur = kernel_mat(kGaussian5), k_sharpen = kernel_mat(kSharpen3);
        Stats blur, sharpen, edges;

        for (const auto& f : files) {
            // Ours
            const GrayImage img = load_gray(f);
            FilterOutputs out(img.width, img.height);
            const ImageView view = whole_image_view(img);
            for (int y = 0; y < img.height; ++y)
                filter_row(view, y, out.blur.row(y), out.sharpen.row(y), out.edges.row(y));

            // OpenCV reference (exact float results, before rounding)
            const cv::Mat src = cv::imread(f, cv::IMREAD_GRAYSCALE);
            cv::Mat ref_blur, ref_sharpen, gx, gy, ref_edges;
            cv::filter2D(src, ref_blur, CV_32F, k_blur, cv::Point(-1, -1), 0, cv::BORDER_REPLICATE);
            cv::filter2D(src, ref_sharpen, CV_32F, k_sharpen, cv::Point(-1, -1), 0, cv::BORDER_REPLICATE);
            cv::Sobel(src, gx, CV_32F, 1, 0, 3, 1, 0, cv::BORDER_REPLICATE);
            cv::Sobel(src, gy, CV_32F, 0, 1, 3, 1, 0, cv::BORDER_REPLICATE);
            cv::magnitude(gx, gy, ref_edges);

            compare(out.blur, ref_blur, blur);
            compare(out.sharpen, ref_sharpen, sharpen);
            compare(out.edges, ref_edges, edges);
        }

        bool ok = true;
        std::printf("  %-14s %14s %16s %16s\n", "filter", "identical", "rounding ties", "real mismatches");
        auto report = [&](const char* name, const Stats& s) {
            ok = ok && s.mismatches == 0;
            std::printf("  %-14s %13.4f%% %15.4f%% %16lld   %s\n", name, 100.0 * s.identical / s.pixels,
                        100.0 * s.ties / s.pixels, s.mismatches, s.mismatches == 0 ? "PASS" : "FAIL");
        };
        report("Gaussian blur", blur);
        report("Sharpen", sharpen);
        report("Sobel edges", edges);
        std::printf("\n%s\n", ok ? "VALIDATION PASSED: our convolution matches OpenCV "
                                   "(only x.5 rounding-convention ties differ)."
                                 : "VALIDATION FAILED");
        return ok ? 0 : 1;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "Error: %s\n", e.what());
        return 1;
    }
}
