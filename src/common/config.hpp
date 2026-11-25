// config.hpp
// Project-wide default settings. Every program can override these with command-line options.
#pragma once

namespace pdc {

// Folder that holds the extracted NIH ChestX-ray14 PNG images.
constexpr const char* kDefaultInputDir = "E:/X-Ray Dataset/images";

// Number of images processed in the main experiments.
constexpr int kDefaultImageCount = 10000;

// Folder where sample output images are written.
constexpr const char* kDefaultOutputDir = "output";

}  // namespace pdc
