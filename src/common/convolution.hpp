// convolution.hpp
// The convolution code shared by ALL versions (sequential, OpenMP, MPI, hybrid).
// Only the way rows/images are divided among workers differs between versions.
#pragma once

#include <cstdint>

#include "image.hpp"
#include "kernels.hpp"

namespace pdc {

// A read-only window onto (part of) an image.
// It holds rows [first_row, first_row + num_rows) of an image whose full height is full_height.
//  - Sequential / OpenMP versions view the whole image.
//  - The MPI strip version views one strip plus its halo rows received from neighbours.
struct ImageView {
    const uint8_t* data = nullptr;  // first row held in this window
    int width = 0;
    int full_height = 0;            // height of the complete image (for border handling)
    int first_row = 0;              // global index of data's first row
    int num_rows = 0;               // number of rows held in this window

    // Pointer to global row y. Rows outside the image are clamped to the nearest
    // edge row ("replicate" border). The row must be present in this window.
    const uint8_t* row(int y) const;
};

// View covering a complete image.
ImageView whole_image_view(const GrayImage& img);

// Computes output row y (global index) of all three filters:
//   blur_out[x]    = Gaussian 5x5 blur
//   sharpen_out[x] = sharpened pixel
//   edges_out[x]   = Sobel edge magnitude
// Requires rows y-2 .. y+2 (clamped to the image) to be available in src.
void filter_row(const ImageView& src, int y,
                uint8_t* blur_out, uint8_t* sharpen_out, uint8_t* edges_out);

}  // namespace pdc
