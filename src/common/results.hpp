// results.hpp
// Recording the outcome of a run: printed summary + one row in a CSV file.
#pragma once

#include <cstdint>
#include <string>

#include "timer.hpp"

namespace pdc {

struct RunResult {
    std::string version;   // sequential, openmp, mpi_batch, mpi_strips
    int procs = 1;         // MPI processes
    int threads = 1;       // OpenMP threads per process
    int images = 0;        // images processed
    std::string mode;      // OpenMP work sharing: rows / images (empty if not applicable)
    std::string schedule;  // OpenMP schedule (empty if not applicable)
    PhaseTimes times;      // read / compute / comm / write breakdown
                           // (when threads work on different images at once, read/compute
                           //  are the AVERAGE busy time per thread, so they stay comparable)
    double total = 0.0;    // end-to-end wall-clock time
    uint64_t checksum = 0; // dataset fingerprint (must equal the sequential one)
    std::string label;
};

// Prints a readable summary to stdout.
void print_result(const RunResult& r);

// Appends the result as one CSV row (writes the header if the file is new).
void append_csv(const std::string& path, const RunResult& r);

}  // namespace pdc
