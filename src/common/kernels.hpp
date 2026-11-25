// kernels.hpp
// The convolution kernels (small weight grids) used by the three filters.
// Integer weights keep the arithmetic exact, so every parallel version
// produces bit-identical output to the sequential version.
#pragma once

#include <array>

namespace pdc {

struct Kernel {
    const char* name;
    int size;                     // kernel is size x size (3 or 5)
    int radius;                   // size / 2: neighbours needed on each side
    int divisor;                  // result = weighted sum / divisor
    std::array<int, 25> weights;  // row-major, only size*size entries are used
};

// Gaussian blur 5x5: binomial weights (1 4 6 4 1) x (1 4 6 4 1), total 256.
constexpr Kernel kGaussian5 = {
    "gaussian_5x5", 5, 2, 256,
    {1,  4,  6,  4, 1,
     4, 16, 24, 16, 4,
     6, 24, 36, 24, 6,
     4, 16, 24, 16, 4,
     1,  4,  6,  4, 1}};

// Sharpen 3x3: boosts the centre pixel relative to its 4 neighbours, total 1.
constexpr Kernel kSharpen3 = {
    "sharpen_3x3", 3, 1, 1,
    { 0, -1,  0,
     -1,  5, -1,
      0, -1,  0}};

// Sobel X: responds to vertical edges (brightness change left -> right).
constexpr Kernel kSobelX = {
    "sobel_x", 3, 1, 1,
    {-1, 0, 1,
     -2, 0, 2,
     -1, 0, 1}};

// Sobel Y: responds to horizontal edges (brightness change top -> bottom).
constexpr Kernel kSobelY = {
    "sobel_y", 3, 1, 1,
    {-1, -2, -1,
      0,  0,  0,
      1,  2,  1}};

// Largest kernel radius. A strip of rows needs this many extra "halo" rows
// above and below it to compute its own rows correctly.
constexpr int kMaxRadius = 2;

}  // namespace pdc
