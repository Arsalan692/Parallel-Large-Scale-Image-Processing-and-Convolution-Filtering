// checksum.hpp
// Fingerprints of the filter outputs, used to prove every parallel version
// produces exactly the same pixels as the sequential version.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#include "image.hpp"

namespace pdc {

constexpr uint64_t kFnvOffset = 1469598103934665603ULL;
constexpr uint64_t kFnvPrime = 1099511628211ULL;

// FNV-1a 64-bit hash of a byte buffer (continues from `hash`).
inline uint64_t fnv1a64(const uint8_t* data, size_t n, uint64_t hash = kFnvOffset) {
    for (size_t i = 0; i < n; ++i) {
        hash ^= data[i];
        hash *= kFnvPrime;
    }
    return hash;
}

// Fingerprint of one image's three outputs.
inline uint64_t outputs_checksum(const FilterOutputs& out) {
    uint64_t h = fnv1a64(out.blur.pixels.data(), out.blur.pixels.size());
    h = fnv1a64(out.sharpen.pixels.data(), out.sharpen.pixels.size(), h);
    h = fnv1a64(out.edges.pixels.data(), out.edges.pixels.size(), h);
    return h;
}

// Dataset fingerprint = SUM of per-image fingerprints (wrapping modulo 2^64).
// A sum does not depend on processing order, so MPI processes can combine
// their partial sums with MPI_Reduce(MPI_SUM) and get the same value.
inline uint64_t combine_checksums(uint64_t a, uint64_t b) { return a + b; }

// 16-digit hexadecimal text of a checksum.
inline std::string to_hex(uint64_t value) {
    static const char digits[] = "0123456789abcdef";
    std::string s(16, '0');
    for (int i = 15; i >= 0; --i, value >>= 4) s[i] = digits[value & 0xF];
    return s;
}

}  // namespace pdc
