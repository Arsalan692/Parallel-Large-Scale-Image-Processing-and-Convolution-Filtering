// image_io.cpp
#include "image_io.hpp"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <stdexcept>

#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>

namespace fs = std::filesystem;

namespace pdc {

std::vector<std::string> list_images(const std::string& dir, int count) {
    if (!fs::is_directory(dir)) throw std::runtime_error("Input folder not found: " + dir);

    std::vector<std::string> files;
    for (const auto& entry : fs::directory_iterator(dir)) {
        if (!entry.is_regular_file()) continue;
        std::string ext = entry.path().extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (ext == ".png") files.push_back(entry.path().string());
    }
    std::sort(files.begin(), files.end());
    if (count > 0 && static_cast<int>(files.size()) > count) files.resize(count);
    return files;
}

GrayImage load_gray(const std::string& path) {
    cv::Mat mat = cv::imread(path, cv::IMREAD_GRAYSCALE);
    if (mat.empty()) throw std::runtime_error("Could not read image: " + path);

    GrayImage img(mat.cols, mat.rows);
    for (int y = 0; y < mat.rows; ++y) std::memcpy(img.row(y), mat.ptr<uint8_t>(y), mat.cols);
    return img;
}

void save_gray(const std::string& path, const GrayImage& img) {
    // Wrap our buffer in a cv::Mat header (no copy) so OpenCV can encode it.
    cv::Mat mat(img.height, img.width, CV_8UC1, const_cast<uint8_t*>(img.pixels.data()));
    if (!cv::imwrite(path, mat)) throw std::runtime_error("Could not write image: " + path);
}

void save_outputs(const std::string& dir, const std::string& input_path, const FilterOutputs& out) {
    fs::create_directories(dir);
    const std::string stem = (fs::path(dir) / fs::path(input_path).stem()).string();
    save_gray(stem + "_blur.png", out.blur);
    save_gray(stem + "_sharpen.png", out.sharpen);
    save_gray(stem + "_edges.png", out.edges);
}

}  // namespace pdc
