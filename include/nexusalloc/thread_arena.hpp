#pragma once

#include <array>
#include <cstddef>
#include <vector>

#include "nexusalloc/atomic_stack.hpp"
#include "nexusalloc/hugepage_provider.hpp"
#include "nexusalloc/internal/alignment.hpp"
#include "nexusalloc/internal/size_class.hpp"
#include "nexusalloc/slab.hpp"

namespace nexusalloc {

// Thread-local arena for fast-path allocations
// Each thread has its own arena with slabs for each size class
class ThreadArena {
 public:
  // Thread-local singleton instance
  static ThreadArena& get() noexcept {
    thread_local ThreadArena arena;
    return arena;
  }

  [[nodiscard]] void* allocate(size_t size) noexcept {
    // Treat size 0 as minimum allocation (matches jemalloc behavior)
    // SizeClass::index(0) returns 0, which maps to 16 bytes
    if (internal::SizeClass::is_large(size)) {
      return allocate_large(size);
    }

    size_t class_idx = internal::SizeClass::index(size);
    auto& bin = bins_[class_idx];

    // Try current slab first
    if (bin.current_slab.valid()) {
      void* ptr = bin.current_slab.allocate();
      if (ptr != nullptr) {
        return ptr;
      }
      // Current slab is full, move to full list
      bin.full_slabs.push_back(std::move(bin.current_slab));
      bin.current_slab = internal::SlabWrapper{};
    }

    // Try partial slabs
    if (!bin.partial_slabs.empty()) {
      bin.current_slab = std::move(bin.partial_slabs.back());
      bin.partial_slabs.pop_back();
      return bin.current_slab.allocate();
    }

    // Need a new chunk
    void* chunk = request_chunk();
    if (chunk == nullptr) {
      return nullptr;  // Out of memory
    }

    // Create new slab for this size class using compile-time dispatch
    bin.current_slab = internal::SlabWrapper(class_idx, chunk);
    return bin.current_slab.allocate();
  }

  void deallocate(void* ptr, size_t size) noexcept {
    if (ptr == nullptr) return;

    if (internal::SizeClass::is_large(size)) {
      deallocate_large(ptr, size);
      return;
    }

    size_t class_idx = internal::SizeClass::index(size);
    auto& bin = bins_[class_idx];

    if (bin.current_slab.valid() && bin.current_slab.contains(ptr)) {
      bin.current_slab.deallocate(ptr);
      return;
    }

    for (auto& slab : bin.partial_slabs) {
      if (slab.contains(ptr)) {
        slab.deallocate(ptr);
        return;
      }
    }

    for (size_t i = 0; i < bin.full_slabs.size(); ++i) {
      if (bin.full_slabs[i].contains(ptr)) {
        bin.full_slabs[i].deallocate(ptr);
        // Move to partial list since it now has free blocks
        bin.partial_slabs.push_back(std::move(bin.full_slabs[i]));
        bin.full_slabs.erase(bin.full_slabs.begin() + static_cast<ptrdiff_t>(i));
        return;
      }
    }

    // Pointer not found in any slab - undefined behavior, silently ignore
  }

  ~ThreadArena() {
    for (auto& bin : bins_) {
      if (bin.current_slab.valid()) {
        return_chunk(bin.current_slab.base());
      }
      for (auto& slab : bin.partial_slabs) {
        return_chunk(slab.base());
      }
      for (auto& slab : bin.full_slabs) {
        return_chunk(slab.base());
      }
    }
  }

 private:
  ThreadArena() = default;

  // Request a new chunk from the global pool or OS
  [[nodiscard]] void* request_chunk() noexcept {
    void* chunk = global_page_stack().pop();
    if (chunk != nullptr) {
      return chunk;
    }

    return HugepageProvider::allocate_chunk();
  }

  void return_chunk(void* chunk) noexcept {
    if (chunk != nullptr) {
      global_page_stack().push(chunk);
    }
  }

  [[nodiscard]] void* allocate_large(size_t size) noexcept {
    size_t aligned_size = internal::align_up(size, PageTraits::kRegularPageSize);
    void* ptr =
        mmap(nullptr, aligned_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (ptr == MAP_FAILED) {
      return nullptr;
    }
    return ptr;
  }

  void deallocate_large(void* ptr, size_t size) noexcept {
    size_t aligned_size = internal::align_up(size, PageTraits::kRegularPageSize);
    munmap(ptr, aligned_size);
  }

  struct alignas(internal::kCacheLineSize) SizeClassBin {
    internal::SlabWrapper current_slab{};
    std::vector<internal::SlabWrapper> partial_slabs;
    std::vector<internal::SlabWrapper> full_slabs;
  };

  std::array<SizeClassBin, internal::SizeClass::kNumClasses> bins_;
};

}  // namespace nexusalloc
