// image.hpp
// Basic image containers used by every version of the program.
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace pdc {

// An 8-bit grayscale image stored row by row (row-major order).
struct GrayImage {
    int width = 0;
    int height = 0;
    std::vector<uint8_t> pixels;  // width * height values

    GrayImage() = default;
    GrayImage(int w, int h) : width(w), height(h), pixels(static_cast<size_t>(w) * h) {}

    uint8_t* row(int y) { return pixels.data() + static_cast<size_t>(y) * width; }
    const uint8_t* row(int y) const { return pixels.data() + static_cast<size_t>(y) * width; }
};

// The three filtered results produced for every input image.
struct FilterOutputs {
    GrayImage blur;     // Gaussian blur 5x5
    GrayImage sharpen;  // Sharpen 3x3
    GrayImage edges;    // Sobel edge magnitude

    FilterOutputs() = default;
    FilterOutputs(int w, int h) : blur(w, h), sharpen(w, h), edges(w, h) {}
};

}  // namespace pdc
