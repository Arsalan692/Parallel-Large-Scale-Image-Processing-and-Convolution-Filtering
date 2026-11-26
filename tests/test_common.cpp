// test_common.cpp
// Unit tests for the shared code in src/common.
// Run: scripts\run.ps1 test_common
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <random>
#include <string>
#include <vector>

#include "common/checksum.hpp"
#include "common/config.hpp"
#include "common/convolution.hpp"
#include "common/image_io.hpp"
#include "common/kernels.hpp"
#include "common/options.hpp"
#include "common/partition.hpp"

using namespace pdc;
namespace fs = std::filesystem;

static int g_failures = 0;

#define CHECK(cond)                                                          \
    do {                                                                     \
        if (!(cond)) {                                                       \
            std::printf("    FAILED: %s  (line %d)\n", #cond, __LINE__);     \
            ++g_failures;                                                    \
        }                                                                    \
    } while (0)

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static GrayImage random_image(int w, int h, unsigned seed) {
    std::mt19937 rng(seed);
    std::uniform_int_distribution<int> dist(0, 255);
    GrayImage img(w, h);
    for (auto& p : img.pixels) p = static_cast<uint8_t>(dist(rng));
    return img;
}

// Runs filter_row over every row of a whole image.
static FilterOutputs filter_whole(const GrayImage& img) {
    FilterOutputs out(img.width, img.height);
    const ImageView view = whole_image_view(img);
    for (int y = 0; y < img.height; ++y)
        filter_row(view, y, out.blur.row(y), out.sharpen.row(y), out.edges.row(y));
    return out;
}

// Deliberately simple reference convolution (clamps every coordinate), written
// independently of convolution.cpp, to check the optimised version.
static int ref_sum(const GrayImage& img, int y, int x, const Kernel& k) {
    int sum = 0;
    for (int i = 0; i < k.size; ++i)
        for (int j = 0; j < k.size; ++j) {
            const int yy = std::clamp(y + i - k.radius, 0, img.height - 1);
            const int xx = std::clamp(x + j - k.radius, 0, img.width - 1);
            sum += k.weights[i * k.size + j] * img.pixels[yy * img.width + xx];
        }
    return sum;
}

static FilterOutputs reference_filters(const GrayImage& img) {
    FilterOutputs out(img.width, img.height);
    auto u8 = [](int v) { return static_cast<uint8_t>(std::clamp(v, 0, 255)); };
    for (int y = 0; y < img.height; ++y)
        for (int x = 0; x < img.width; ++x) {
            const int i = y * img.width + x;
            const int gx = ref_sum(img, y, x, kSobelX), gy = ref_sum(img, y, x, kSobelY);
            out.blur.pixels[i] = u8((ref_sum(img, y, x, kGaussian5) + 128) / 256);
            out.sharpen.pixels[i] = u8(ref_sum(img, y, x, kSharpen3));
            out.edges.pixels[i] = u8(static_cast<int>(std::sqrt(static_cast<float>(gx * gx + gy * gy)) + 0.5f));
        }
    return out;
}

static bool same(const FilterOutputs& a, const FilterOutputs& b) {
    return a.blur.pixels == b.blur.pixels && a.sharpen.pixels == b.sharpen.pixels &&
           a.edges.pixels == b.edges.pixels;
}

static int px(const GrayImage& img, int y, int x) { return img.pixels[y * img.width + x]; }

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

static void test_kernel_weights() {
    auto total = [](const Kernel& k) {
        int s = 0;
        for (int i = 0; i < k.size * k.size; ++i) s += k.weights[i];
        return s;
    };
    CHECK(total(kGaussian5) == kGaussian5.divisor);  // blur keeps brightness
    CHECK(total(kSharpen3) == 1);                    // sharpen keeps brightness
    CHECK(total(kSobelX) == 0 && total(kSobelY) == 0);  // no edge on flat areas
    CHECK(kMaxRadius == kGaussian5.radius);
}

static void test_flat_image() {
    // A completely flat image: blur and sharpen keep the value, no edges anywhere (borders included).
    GrayImage img(9, 7);
    std::fill(img.pixels.begin(), img.pixels.end(), 100);
    const FilterOutputs out = filter_whole(img);
    for (size_t i = 0; i < img.pixels.size(); ++i) {
        CHECK(out.blur.pixels[i] == 100);
        CHECK(out.sharpen.pixels[i] == 100);
        CHECK(out.edges.pixels[i] == 0);
    }
}

static void test_hand_computed_values() {
    // 7x7 black image with a single pixel of value 100 at the centre (3,3).
    GrayImage img(7, 7);
    img.pixels[3 * 7 + 3] = 100;
    const FilterOutputs out = filter_whole(img);

    CHECK(px(out.blur, 3, 3) == 14);      // (100*36 + 128) / 256 = 14
    CHECK(px(out.blur, 3, 4) == 9);       // (100*24 + 128) / 256 = 9
    CHECK(px(out.sharpen, 3, 3) == 255);  // 5*100 = 500 -> clamped to 255
    CHECK(px(out.sharpen, 2, 3) == 0);    // -100 -> clamped to 0
    CHECK(px(out.edges, 3, 3) == 0);      // symmetric around the centre: gx = gy = 0
    CHECK(px(out.edges, 3, 2) == 200);    // left neighbour: gx = 2*100, gy = 0
    CHECK(px(out.edges, 2, 2) == 141);    // diagonal: gx = gy = 100 -> sqrt(20000) = 141.4
}

static void test_matches_reference() {
    // Random images of awkward sizes, including tiny ones narrower than the kernel.
    const int sizes[][2] = {{37, 23}, {64, 50}, {5, 5}, {3, 8}, {1, 1}, {2, 9}, {128, 3}};
    unsigned seed = 1;
    for (const auto& s : sizes) {
        const GrayImage img = random_image(s[0], s[1], seed++);
        CHECK(same(filter_whole(img), reference_filters(img)));
    }
}

static void test_strips_with_halo_match_whole_image() {
    // Simulates the MPI strip version: each "process" only holds its strip
    // plus kMaxRadius halo rows above/below, yet must produce identical rows.
    const GrayImage img = random_image(64, 50, 42);
    const FilterOutputs whole = filter_whole(img);

    for (int parts : {1, 2, 3, 4, 7, 8}) {
        FilterOutputs assembled(img.width, img.height);
        for (int p = 0; p < parts; ++p) {
            const Range own = split_range(img.height, parts, p);
            const int halo_begin = std::max(0, own.begin - kMaxRadius);
            const int halo_end = std::min(img.height, own.end + kMaxRadius);

            // Copy only the strip + halo rows into a separate buffer (like an MPI receive buffer).
            std::vector<uint8_t> buffer(img.row(halo_begin), img.row(0) + halo_end * img.width);
            const ImageView view{buffer.data(), img.width, img.height, halo_begin, halo_end - halo_begin};

            for (int y = own.begin; y < own.end; ++y)
                filter_row(view, y, assembled.blur.row(y), assembled.sharpen.row(y), assembled.edges.row(y));
        }
        CHECK(same(assembled, whole));
    }
}

static void test_split_range() {
    for (int total : {0, 1, 7, 10, 1024, 10000})
        for (int parts : {1, 2, 3, 4, 8}) {
            int expected_begin = 0, min_size = total, max_size = 0;
            for (int p = 0; p < parts; ++p) {
                const Range r = split_range(total, parts, p);
                CHECK(r.begin == expected_begin);  // contiguous, no gaps or overlaps
                expected_begin = r.end;
                min_size = std::min(min_size, r.size());
                max_size = std::max(max_size, r.size());
            }
            CHECK(expected_begin == total);       // everything covered
            CHECK(max_size - min_size <= 1);      // balanced
        }
}

static void test_checksum() {
    GrayImage img = random_image(32, 32, 7);
    const FilterOutputs a = filter_whole(img);
    const FilterOutputs b = filter_whole(img);
    CHECK(outputs_checksum(a) == outputs_checksum(b));  // deterministic

    img.pixels[100] ^= 1;  // change one bit of the input
    CHECK(outputs_checksum(filter_whole(img)) != outputs_checksum(a));

    CHECK(combine_checksums(5, 9) == combine_checksums(9, 5));  // order independent
    CHECK(to_hex(0x1234abcdULL) == "000000001234abcd");
}

static void test_options() {
    {
        const char* argv[] = {"prog", "--count", "500", "--threads", "4", "--schedule", "dynamic",
                              "--save", "3", "--csv", "r.csv", "--label", "hi", "--mode", "images"};
        Options opt;
        std::string err;
        CHECK(parse_options(15, const_cast<char**>(argv), opt, err));
        CHECK(opt.count == 500 && opt.threads == 4 && opt.schedule == "dynamic" && opt.mode == "images");
        CHECK(opt.save == 3 && opt.csv == "r.csv" && opt.label == "hi");
        CHECK(opt.input_dir == kDefaultInputDir);
    }
    {
        const char* argv[] = {"prog", "--count", "abc"};
        Options opt;
        std::string err;
        CHECK(!parse_options(3, const_cast<char**>(argv), opt, err));
    }
    {
        const char* argv[] = {"prog", "--bogus", "1"};
        Options opt;
        std::string err;
        CHECK(!parse_options(3, const_cast<char**>(argv), opt, err));
    }
}

static void test_image_io() {
    // Save -> load round trip must be lossless (PNG).
    const GrayImage img = random_image(40, 30, 3);
    const std::string tmp = (fs::temp_directory_path() / "pdc_roundtrip_test.png").string();
    save_gray(tmp, img);
    const GrayImage back = load_gray(tmp);
    CHECK(back.width == 40 && back.height == 30 && back.pixels == img.pixels);
    fs::remove(tmp);

    // Real dataset (skipped if the folder is not present on this machine).
    if (!fs::is_directory(kDefaultInputDir)) {
        std::printf("    (dataset folder not found - dataset checks skipped)\n");
        return;
    }
    const auto files = list_images(kDefaultInputDir, 5);
    CHECK(files.size() == 5);
    CHECK(std::is_sorted(files.begin(), files.end()));
    const GrayImage xray = load_gray(files[0]);
    CHECK(xray.width == 1024 && xray.height == 1024);
}

// ---------------------------------------------------------------------------

int main() {
    struct Test { const char* name; void (*fn)(); };
    const Test tests[] = {
        {"kernel weights", test_kernel_weights},
        {"flat image", test_flat_image},
        {"hand-computed values", test_hand_computed_values},
        {"matches reference convolution", test_matches_reference},
        {"strips + halo == whole image", test_strips_with_halo_match_whole_image},
        {"split_range partitioning", test_split_range},
        {"checksum", test_checksum},
        {"command-line options", test_options},
        {"image I/O", test_image_io},
    };

    for (const auto& t : tests) {
        const int before = g_failures;
        t.fn();
        std::printf("[%s] %s\n", g_failures == before ? "PASS" : "FAIL", t.name);
    }
    std::printf("\n%s (%d failed check(s))\n", g_failures == 0 ? "ALL TESTS PASSED" : "SOME TESTS FAILED",
                g_failures);
    return g_failures == 0 ? 0 : 1;
}
