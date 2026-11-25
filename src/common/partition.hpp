// partition.hpp
// Block partitioning: splits N items (images or image rows) into P nearly equal parts.
// Used by the MPI versions to decide which images / rows each process handles.
#pragma once

namespace pdc {

struct Range {
    int begin = 0;  // first item (inclusive)
    int end = 0;    // last item (exclusive)
    int size() const { return end - begin; }
};

// Part `index` (0-based) of `total` items split into `parts` parts.
// The first (total % parts) parts get one extra item, so sizes differ by at most 1.
inline Range split_range(int total, int parts, int index) {
    const int base = total / parts;
    const int extra = total % parts;
    const int begin = index * base + (index < extra ? index : extra);
    const int size = base + (index < extra ? 1 : 0);
    return Range{begin, begin + size};
}

}  // namespace pdc
