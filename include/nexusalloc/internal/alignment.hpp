#pragma once

#include <cstddef>
#include <cstdint>

namespace nexusalloc::internal {

// Align a value up to the specified alignment
template <typename T>
[[nodiscard]] constexpr T align_up(T value, T alignment) noexcept {
  return (value + alignment - 1) & ~(alignment - 1);
}

// Check if a pointer is aligned to the specified alignment
template <typename T>
[[nodiscard]] constexpr bool is_aligned(T* ptr, size_t alignment) noexcept {
  return (reinterpret_cast<uintptr_t>(ptr) & (alignment - 1)) == 0;
}

// Check if a size is aligned to the specified alignment
[[nodiscard]] constexpr bool is_size_aligned(size_t size, size_t alignment) noexcept {
  return (size & (alignment - 1)) == 0;
}

// Common alignment constants
inline constexpr size_t kMinAlignment = 16;   // SIMD-friendly
inline constexpr size_t kCacheLineSize = 64;  // Prevent false sharing
inline constexpr size_t kPointerSize = sizeof(void*);

}  // namespace nexusalloc::internal
