#pragma once

#include "nexusalloc/allocator.hpp"
#include "nexusalloc/thread_arena.hpp"

namespace nexusalloc {

inline void initialize() { HugepageProvider::lock_memory(); }

[[nodiscard]] inline void* allocate(size_t size) noexcept {
  return ThreadArena::get().allocate(size);
}
inline void deallocate(void* ptr, size_t size) noexcept {
  ThreadArena::get().deallocate(ptr, size);
}

}  // namespace nexusalloc
