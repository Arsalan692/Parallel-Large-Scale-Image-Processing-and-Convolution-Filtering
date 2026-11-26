// options.cpp
#include "options.hpp"

#include <cstdio>
#include <stdexcept>

namespace pdc {

namespace {

bool parse_int(const std::string& text, int min_value, int& out) {
    try {
        size_t used = 0;
        const int value = std::stoi(text, &used);
        if (used != text.size() || value < min_value) return false;
        out = value;
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

}  // namespace

bool parse_options(int argc, char** argv, Options& opt, std::string& error) {
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            opt.help = true;
            continue;
        }

        // Every other option takes exactly one value.
        if (i + 1 >= argc) {
            error = "missing value for " + arg;
            return false;
        }
        const std::string value = argv[++i];

        if (arg == "--input") {
            opt.input_dir = value;
        } else if (arg == "--count") {
            if (!parse_int(value, 1, opt.count)) { error = "--count needs a positive integer"; return false; }
        } else if (arg == "--save") {
            if (!parse_int(value, 0, opt.save)) { error = "--save needs an integer >= 0"; return false; }
        } else if (arg == "--out") {
            opt.output_dir = value;
        } else if (arg == "--csv") {
            opt.csv = value;
        } else if (arg == "--threads") {
            if (!parse_int(value, 1, opt.threads)) { error = "--threads needs a positive integer"; return false; }
        } else if (arg == "--schedule") {
            if (value != "static" && value != "dynamic") { error = "--schedule must be static or dynamic"; return false; }
            opt.schedule = value;
        } else if (arg == "--mode") {
            if (value != "rows" && value != "images") { error = "--mode must be rows or images"; return false; }
            opt.mode = value;
        } else if (arg == "--label") {
            opt.label = value;
        } else {
            error = "unknown option " + arg;
            return false;
        }
    }
    return true;
}

void print_usage(const char* program) {
    std::printf(
        "Usage: %s [options]\n"
        "  --input <folder>    folder with .png X-ray images (default: %s)\n"
        "  --count <n>         number of images to process (default: %d)\n"
        "  --save <n>          save the outputs of the first n images (default: 0)\n"
        "  --out <folder>      folder for saved outputs (default: %s)\n"
        "  --csv <file>        append the timing results to this CSV file\n"
        "  --threads <n>       OpenMP threads per process (default: OpenMP default)\n"
        "  --schedule <kind>   OpenMP loop schedule: static or dynamic (default: static)\n"
        "  --mode <kind>       what OpenMP threads share: rows (of each image) or images (default: rows)\n"
        "  --label <text>      free-text note stored in the CSV\n"
        "  --help              show this message\n",
        program, kDefaultInputDir, kDefaultImageCount, kDefaultOutputDir);
}

}  // namespace pdc
