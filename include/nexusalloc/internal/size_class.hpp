#pragma once

#include <array>
#include <cstddef>

#include "nexusalloc/internal/alignment.hpp"

namespace nexusalloc::internal {

// Size class manager for segregated free lists
// Small sizes: 8-byte increments from 16 to 256 (16 classes)
// Large sizes: powers of 2 from 512 to 65536 (8 classes)
class SizeClass {
 public:
  static constexpr size_t kNumSmallClasses = 16;  // 16, 32, 48, ..., 256
  static constexpr size_t kNumLargeClasses = 8;   // 512, 1024, 2048, ..., 65536
  static constexpr size_t kNumClasses = kNumSmallClasses + kNumLargeClasses;

  static constexpr size_t kMinBlockSize = 16;    // Minimum for alignment + next pointer
  static constexpr size_t kMaxSmallSize = 256;   // Max size for small classes
  static constexpr size_t kMaxSlabSize = 65536;  // 64KB - max for slab allocation

  // Get size class index for a given allocation size
  // Returns kNumClasses if size is too large for slab allocation
  [[nodiscard]] static constexpr size_t index(size_t size) noexcept {
    if (size == 0) return 0;

    // Ensure minimum size
    size = (size < kMinBlockSize) ? kMinBlockSize : size;

    if (size <= kMaxSmallSize) {
      // Small classes: kMinBlockSize-byte increments
      // Round up to nearest kMinBlockSize bytes, then convert to index
      size_t aligned = internal::align_up(size, size_t{kMinBlockSize});
      return (aligned / kMinBlockSize) - 1;  // 16->0, 32->1, 48->2, ..., 256->15
    }

    if (size <= kMaxSlabSize) {
      // Use CLZ (Count Leading Zeros) for O(1) power-of-2 index calculation
      // For large sizes, we need to find the smallest power of 2 >= size
      // log2_ceil(size) = 64 - clz(size - 1) for size > 1
      // Large class index = log2_ceil(size) - 9 (since 512 = 2^9 is first large class)
      //
      // Example: size=513 -> clz(512) = 55 -> 64-55=9 -> 9-9+16 = 16 (512 class)
      // Example: size=1024 -> clz(1023) = 54 -> 64-54=10 -> 10-9+16 = 17 (1024 class)
      unsigned int log2_ceil = 64 - static_cast<unsigned int>(__builtin_clzll(size - 1));
      return kNumSmallClasses + log2_ceil - 9;
    }

    // Too large for slab allocation
    return kNumClasses;
  }

  // Get the actual block size for a size class
  [[nodiscard]] static constexpr size_t block_size(size_t idx) noexcept {
    if (idx >= kNumClasses) return 0;
    return kSizes[idx];
  }

  // Check if size requires direct mmap (too large for slabs)
  [[nodiscard]] static constexpr bool is_large(size_t size) noexcept { return size > kMaxSlabSize; }

  // Get all size class sizes (for debugging/stats)
  [[nodiscard]] static constexpr const std::array<size_t, kNumClasses>& sizes() noexcept {
    return kSizes;
  }

 private:
  // Pre-computed size class values
  static constexpr std::array<size_t, kNumClasses> kSizes = []() {
    std::array<size_t, kNumClasses> arr{};

    // Small classes: 16, 32, 48, ..., 256 (16-byte increments)
    for (size_t i = 0; i < kNumSmallClasses; ++i) {
      arr[i] = (i + 1) * 16;
    }

    // Large classes: 512, 1024, 2048, ..., 65536 (powers of 2)
    size_t power = 512;
    for (size_t i = 0; i < kNumLargeClasses; ++i) {
      arr[kNumSmallClasses + i] = power;
      power *= 2;
    }

    return arr;
  }();
};

}  // namespace nexusalloc::internal
