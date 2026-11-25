// image_io.hpp
// Reading and writing image files. This is the ONLY place OpenCV is used.
#pragma once

#include <string>
#include <vector>

#include "image.hpp"

namespace pdc {

// Sorted list of the .png files in `dir`, limited to the first `count` (count <= 0 means all).
// Sorting makes every run (and every MPI process) see the same image order.
std::vector<std::string> list_images(const std::string& dir, int count);

// Loads an image as 8-bit grayscale (colour images are converted).
GrayImage load_gray(const std::string& path);

// Saves an 8-bit grayscale image (format chosen from the file extension).
void save_gray(const std::string& path, const GrayImage& img);

// Saves the three outputs of one input image as <dir>/<stem>_blur.png, _sharpen.png, _edges.png.
void save_outputs(const std::string& dir, const std::string& input_path, const FilterOutputs& out);

}  // namespace pdc
