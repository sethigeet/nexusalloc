#pragma once

namespace nexusalloc::internal {

// Prefetch for read with high temporal locality (most common case)
inline void prefetch_read(const void* ptr) noexcept {
#if defined(__GNUC__) || defined(__clang__)
  __builtin_prefetch(ptr, 0, 3);
#elif defined(_MSC_VER)
  _mm_prefetch(static_cast<const char*>(ptr), _MM_HINT_T0);
#else
  (void)ptr;
#endif
}

// Prefetch for write with high temporal locality
inline void prefetch_write(void* ptr) noexcept {
#if defined(__GNUC__) || defined(__clang__)
  __builtin_prefetch(ptr, 1, 3);
#elif defined(_MSC_VER)
  _mm_prefetch(static_cast<const char*>(ptr), _MM_HINT_T0);
#else
  (void)ptr;
#endif
}

// Prefetch for read with low temporal locality (streaming)
inline void prefetch_nontemporal(const void* ptr) noexcept {
#if defined(__GNUC__) || defined(__clang__)
  __builtin_prefetch(ptr, 0, 0);
#elif defined(_MSC_VER)
  _mm_prefetch(static_cast<const char*>(ptr), _MM_HINT_NTA);
#else
  (void)ptr;
#endif
}

}  // namespace nexusalloc::internal
