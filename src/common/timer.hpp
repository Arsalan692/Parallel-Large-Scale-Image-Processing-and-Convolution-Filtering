// timer.hpp
// Wall-clock timing helpers.
#pragma once

#include <chrono>

namespace pdc {

// Current wall-clock time in seconds (monotonic clock).
inline double wall_time() {
    using namespace std::chrono;
    return duration<double>(steady_clock::now().time_since_epoch()).count();
}

// Time spent in each part of a run, in seconds.
struct PhaseTimes {
    double read = 0.0;     // loading images from disk
    double compute = 0.0;  // convolution filters
    double comm = 0.0;     // MPI communication (0 for sequential / OpenMP)
    double write = 0.0;    // saving sample outputs
};

}  // namespace pdc
