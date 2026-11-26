// mpi_utils.hpp
// Small MPI helpers shared by the two MPI programs (mpi_batch, mpi_strips).
// Header-only so that non-MPI programs do not need to link against MPI.
#pragma once

#include <mpi.h>

#include <cstdio>
#include <string>
#include <vector>

#include "timer.hpp"

namespace pdc {

// Rank 0 owns the list of image paths; every other process receives a copy.
// The list is packed into one '\n'-separated string so it can be sent with two
// MPI_Bcast calls (first the length, then the characters).
inline std::vector<std::string> broadcast_file_list(const std::vector<std::string>& root_files,
                                                    int rank, MPI_Comm comm) {
    std::string packed;
    if (rank == 0)
        for (const auto& f : root_files) packed += f + '\n';

    int length = static_cast<int>(packed.size());
    MPI_Bcast(&length, 1, MPI_INT, 0, comm);
    packed.resize(length);
    MPI_Bcast(packed.data(), length, MPI_CHAR, 0, comm);

    std::vector<std::string> files;
    size_t start = 0;
    for (size_t pos; (pos = packed.find('\n', start)) != std::string::npos; start = pos + 1)
        files.push_back(packed.substr(start, pos - start));
    return files;
}

// Per-process statistics, gathered on rank 0 to show how the work was balanced.
struct RankStats {
    double items = 0;  // images (batch) or rows per image (strips) handled by this process
    double read = 0, compute = 0, comm = 0;
};

inline void print_rank_table(const RankStats& mine, int rank, int size, MPI_Comm comm,
                             const char* items_label) {
    std::vector<RankStats> all(size);
    MPI_Gather(&mine, 4, MPI_DOUBLE, all.data(), 4, MPI_DOUBLE, 0, comm);
    if (rank != 0) return;
    std::printf("\n  per-process breakdown:\n    rank %10s %10s %10s %10s\n", items_label, "read s",
                "compute s", "comm s");
    for (int r = 0; r < size; ++r)
        std::printf("    %4d %10.0f %10.2f %10.2f %10.2f\n", r, all[r].items, all[r].read, all[r].compute,
                    all[r].comm);
}

}  // namespace pdc
