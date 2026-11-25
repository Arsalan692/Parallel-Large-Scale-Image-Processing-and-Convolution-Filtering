// options.hpp
// Command-line options shared by all programs.
#pragma once

#include <string>

#include "config.hpp"

namespace pdc {

struct Options {
    std::string input_dir = kDefaultInputDir;    // --input <folder>
    int count = kDefaultImageCount;              // --count <n>     images to process
    int save = 0;                                // --save <n>      save outputs of the first n images
    std::string output_dir = kDefaultOutputDir;  // --out <folder>  where saved outputs go
    std::string csv;                             // --csv <file>    append a results row to this CSV
    int threads = 0;                             // --threads <n>   OpenMP threads (0 = default)
    std::string schedule = "static";             // --schedule static|dynamic  OpenMP loop schedule
    std::string label;                           // --label <text>  free note stored in the CSV
    bool help = false;                           // --help
};

// Parses argv into `opt`. Returns false and sets `error` on invalid input.
bool parse_options(int argc, char** argv, Options& opt, std::string& error);

// Prints the list of options.
void print_usage(const char* program);

}  // namespace pdc
