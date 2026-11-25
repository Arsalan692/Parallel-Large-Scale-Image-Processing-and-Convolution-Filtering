// convolution.cpp
// Hand-written 2D convolution. OpenCV is NOT used here; it is only used to read/write files.
#include "convolution.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>

namespace pdc {

const uint8_t* ImageView::row(int y) const {
    y = std::clamp(y, 0, full_height - 1);  // replicate border at top/bottom of the image
    assert(y >= first_row && y < first_row + num_rows && "row not in view: halo too small");
    return data + static_cast<size_t>(y - first_row) * width;
}

ImageView whole_image_view(const GrayImage& img) {
    return ImageView{img.pixels.data(), img.width, img.height, 0, img.height};
}

namespace {

inline uint8_t to_u8(int v) { return static_cast<uint8_t>(std::clamp(v, 0, 255)); }

// Convolution of one pixel: the weighted sum of its neighbourhood.
// rows[i] points to the source row (y - radius + i), already clamped vertically.
// ClampX = false is the fast path for interior columns;
// ClampX = true handles the left/right image borders (replicate border).
template <bool ClampX>
inline int weighted_sum(const uint8_t* const* rows, int x, int width, const Kernel& k) {
    int sum = 0;
    for (int i = 0; i < k.size; ++i) {
        const uint8_t* src_row = rows[i];
        const int* w = &k.weights[static_cast<size_t>(i) * k.size];
        for (int j = 0; j < k.size; ++j) {
            int xx = x + j - k.radius;
            if constexpr (ClampX) xx = std::clamp(xx, 0, width - 1);
            sum += w[j] * src_row[xx];
        }
    }
    return sum;
}

// Applies the three filters to pixel (x) of the current row.
template <bool ClampX>
inline void filter_pixel(const uint8_t* const* rows5, int x, int width,
                         uint8_t* blur_out, uint8_t* sharpen_out, uint8_t* edges_out) {
    const uint8_t* const* rows3 = rows5 + 1;  // 3x3 kernels use rows y-1 .. y+1

    const int blur = weighted_sum<ClampX>(rows5, x, width, kGaussian5);
    const int sharp = weighted_sum<ClampX>(rows3, x, width, kSharpen3);
    const int gx = weighted_sum<ClampX>(rows3, x, width, kSobelX);
    const int gy = weighted_sum<ClampX>(rows3, x, width, kSobelY);

    blur_out[x] = to_u8((blur + kGaussian5.divisor / 2) / kGaussian5.divisor);  // rounded average
    sharpen_out[x] = to_u8(sharp);
    edges_out[x] = to_u8(static_cast<int>(std::sqrt(static_cast<float>(gx * gx + gy * gy)) + 0.5f));
}

}  // namespace

void filter_row(const ImageView& src, int y,
                uint8_t* blur_out, uint8_t* sharpen_out, uint8_t* edges_out) {
    const int width = src.width;

    // Pointers to the 5 source rows around y (needed by the 5x5 kernel).
    const uint8_t* rows5[5];
    for (int i = 0; i < 5; ++i) rows5[i] = src.row(y - kMaxRadius + i);

    // Left border columns, interior columns (no clamping needed), right border columns.
    const int left_end = std::min(kMaxRadius, width);
    const int right_begin = std::max(left_end, width - kMaxRadius);

    for (int x = 0; x < left_end; ++x)
        filter_pixel<true>(rows5, x, width, blur_out, sharpen_out, edges_out);
    for (int x = left_end; x < right_begin; ++x)
        filter_pixel<false>(rows5, x, width, blur_out, sharpen_out, edges_out);
    for (int x = right_begin; x < width; ++x)
        filter_pixel<true>(rows5, x, width, blur_out, sharpen_out, edges_out);
}

}  // namespace pdc
