#pragma once

#include <cstddef>
#include <new>
#include <type_traits>

#include "nexusalloc/thread_arena.hpp"

namespace nexusalloc {

template <typename T>
class NexusAllocator {
 public:
  using value_type = T;
  using size_type = std::size_t;
  using difference_type = std::ptrdiff_t;
  using propagate_on_container_move_assignment = std::true_type;
  using is_always_equal = std::true_type;

  constexpr NexusAllocator() noexcept = default;

  template <typename U>
  constexpr NexusAllocator(const NexusAllocator<U>&) noexcept {}

  [[nodiscard]] T* allocate(size_type n) {
    if (n == 0) {
      return nullptr;
    }

    size_t bytes = n * sizeof(T);
    void* ptr = ThreadArena::get().allocate(bytes);

    if (ptr == nullptr) {
      throw std::bad_alloc();
    }

    return static_cast<T*>(ptr);
  }

  void deallocate(T* ptr, size_type n) noexcept {
    if (ptr == nullptr) return;
    size_t bytes = n * sizeof(T);
    ThreadArena::get().deallocate(ptr, bytes);
  }
};

template <typename T, typename U>
constexpr bool operator==(const NexusAllocator<T>&, const NexusAllocator<U>&) noexcept {
  return true;
}

template <typename T, typename U>
constexpr bool operator!=(const NexusAllocator<T>&, const NexusAllocator<U>&) noexcept {
  return false;
}

}  // namespace nexusalloc
