#pragma once

#include <cstddef>

#include "nexusalloc/hugepage_provider.hpp"
#include "nexusalloc/internal/alignment.hpp"
#include "nexusalloc/internal/bitmap.hpp"
#include "nexusalloc/internal/prefetch.hpp"

namespace nexusalloc::internal {

// Mask to extract the slab base address from any pointer within the slab.
// Since slabs are 2MB (kChunkSize) aligned, this mask clears the lower 21 bits.
inline constexpr uintptr_t kSlabBaseMask = ~(static_cast<uintptr_t>(PageTraits::kChunkSize) - 1);

// Extract the slab base address from a pointer via bitwise masking.
[[nodiscard]] inline void* slab_base_from_ptr(void* ptr) noexcept {
  return reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(ptr) & kSlabBaseMask);
}

template <size_t BlockSize>
class Slab {
  static_assert(BlockSize >= 16, "BlockSize must be at least 16 for alignment");
  static_assert(BlockSize >= sizeof(void*), "BlockSize must fit a pointer");
  static_assert(BlockSize % 16 == 0, "BlockSize must be a multiple of 16");

 public:
  static constexpr size_t kBlockSize = BlockSize;
  static constexpr size_t kChunkSize = PageTraits::kChunkSize;
  static constexpr size_t kBlocksPerSlab = kChunkSize / kBlockSize;

  explicit Slab(void* chunk) noexcept : base_(chunk) {
    if (chunk == nullptr) return;

    char* ptr = static_cast<char*>(chunk);
    for (size_t i = 0; i < kBlocksPerSlab - 1; ++i) {
      void** block = reinterpret_cast<void**>(ptr + i * kBlockSize);
      *block = ptr + (i + 1) * kBlockSize;  // Point to next block
    }
    // Last block points to nullptr
    void** last_block = reinterpret_cast<void**>(ptr + (kBlocksPerSlab - 1) * kBlockSize);
    *last_block = nullptr;

    free_head_ = chunk;
  }

  ~Slab() = default;

  // Non-copyable, non-movable
  Slab(const Slab&) = delete;
  Slab& operator=(const Slab&) = delete;
  Slab(Slab&&) = delete;
  Slab& operator=(Slab&&) = delete;

  [[nodiscard]] void* allocate() noexcept {
    if (free_head_ == nullptr) {
      return nullptr;
    }

    void* block = free_head_;
    void* next = *static_cast<void**>(block);
    if (next != nullptr) {
      prefetch_read(next);  // prefetch the next block for bursty allocations
    }

    free_head_ = next;
    ++allocated_count_;

    size_t idx = block_index(block);
    occupancy_.set(idx);

    return block;
  }

  void deallocate(void* ptr) noexcept {
    if (ptr == nullptr || !contains(ptr)) {
      return;
    }

    size_t idx = block_index(ptr);
    occupancy_.clear(idx);

    *static_cast<void**>(ptr) = free_head_;
    free_head_ = ptr;
    --allocated_count_;
  }

  [[nodiscard]] bool empty() const noexcept { return allocated_count_ == 0; }

  [[nodiscard]] bool full() const noexcept { return free_head_ == nullptr; }

  [[nodiscard]] size_t used_blocks() const noexcept { return allocated_count_; }

  [[nodiscard]] size_t free_blocks() const noexcept { return kBlocksPerSlab - allocated_count_; }

  [[nodiscard]] bool contains(const void* ptr) const noexcept {
    const char* p = static_cast<const char*>(ptr);
    const char* base = static_cast<const char*>(base_);
    return p >= base && p < base + kChunkSize;
  }

  [[nodiscard]] void* base() const noexcept { return base_; }

  [[nodiscard]] const Bitmap<kBlocksPerSlab>& occupancy() const noexcept { return occupancy_; }

 private:
  [[nodiscard]] size_t block_index(const void* ptr) const noexcept {
    return (static_cast<const char*>(ptr) - static_cast<const char*>(base_)) / kBlockSize;
  }

  void* base_{nullptr};
  void* free_head_{nullptr};
  size_t allocated_count_{0};

  // Bitmap for tracking (1 bit per block)
  // Aligned to cache line to prevent false sharing
  alignas(kCacheLineSize) Bitmap<kBlocksPerSlab> occupancy_;
};

// Map size class index to block size at compile time
template <size_t ClassIndex>
struct SizeClassToBlockSize;

// Specializations for small classes (0-15): 16, 32, 48, ..., 256
template <size_t I>
requires(I < 16)
struct SizeClassToBlockSize<I> {
  static constexpr size_t value = (I + 1) * 16;
};

// Specializations for large classes (16-23): 512, 1024, 2048, ..., 65536
template <size_t I>
requires(I >= 16 && I < 24)
struct SizeClassToBlockSize<I> {
  static constexpr size_t value = 512ULL << (I - 16);
};

template <size_t ClassIndex>
inline constexpr size_t kBlockSizeForClass = SizeClassToBlockSize<ClassIndex>::value;

// Macro to generate switch cases for all size classes
// This generates optimal jump-table code that the compiler can fully inline
#define NEXUS_DISPATCH_CASE(idx, slab_ptr, op)                       \
  case idx: {                                                        \
    auto* s = static_cast<Slab<kBlockSizeForClass<idx>>*>(slab_ptr); \
    return s->op;                                                    \
  }

#define NEXUS_CREATE_CASE(idx, chunk)                     \
  case idx: {                                             \
    slab_ptr_ = new Slab<kBlockSizeForClass<idx>>(chunk); \
    return;                                               \
  }

#define NEXUS_DESTROY_CASE(idx, slab_ptr_)                         \
  case idx: {                                                      \
    delete static_cast<Slab<kBlockSizeForClass<idx>>*>(slab_ptr_); \
    break;                                                         \
  }

// clang-format off
#define NEXUS_GENERATE_ALL_CASES(generator, ...)                                                   \
  generator(0, __VA_ARGS__)  \
  generator(1, __VA_ARGS__)  \
  generator(2, __VA_ARGS__)  \
  generator(3, __VA_ARGS__)  \
  generator(4, __VA_ARGS__)  \
  generator(5, __VA_ARGS__)  \
  generator(6, __VA_ARGS__)  \
  generator(7, __VA_ARGS__)  \
  generator(8, __VA_ARGS__)  \
  generator(9, __VA_ARGS__)  \
  generator(10, __VA_ARGS__) \
  generator(11, __VA_ARGS__) \
  generator(12, __VA_ARGS__) \
  generator(13, __VA_ARGS__) \
  generator(14, __VA_ARGS__) \
  generator(15, __VA_ARGS__) \
  generator(16, __VA_ARGS__) \
  generator(17, __VA_ARGS__) \
  generator(18, __VA_ARGS__) \
  generator(19, __VA_ARGS__) \
  generator(20, __VA_ARGS__) \
  generator(21, __VA_ARGS__) \
  generator(22, __VA_ARGS__) \
  generator(23, __VA_ARGS__)
// clang-format on

class SlabWrapper {
 public:
  SlabWrapper() noexcept = default;

  SlabWrapper(size_t class_idx, void* chunk) noexcept : class_idx_(class_idx) {
    if (chunk == nullptr) return;
    create_slab(chunk);
  }

  ~SlabWrapper() { destroy(); }

  SlabWrapper(SlabWrapper&& other) noexcept
      : slab_ptr_(other.slab_ptr_), class_idx_(other.class_idx_) {
    other.slab_ptr_ = nullptr;
  }

  SlabWrapper& operator=(SlabWrapper&& other) noexcept {
    if (this != &other) {
      destroy();
      slab_ptr_ = other.slab_ptr_;
      class_idx_ = other.class_idx_;
      other.slab_ptr_ = nullptr;
    }
    return *this;
  }

  SlabWrapper(const SlabWrapper&) = delete;
  SlabWrapper& operator=(const SlabWrapper&) = delete;

  [[nodiscard]] bool valid() const noexcept { return slab_ptr_ != nullptr; }
  [[nodiscard]] size_t class_index() const noexcept { return class_idx_; }

  [[nodiscard]] void* allocate() noexcept {
    if (slab_ptr_ == nullptr) return nullptr;
    switch (class_idx_) {
      NEXUS_GENERATE_ALL_CASES(NEXUS_DISPATCH_CASE, slab_ptr_, allocate())
      default:
        return nullptr;
    }
  }

  void deallocate(void* ptr) noexcept {
    if (slab_ptr_ == nullptr) return;
    switch (class_idx_) {
      NEXUS_GENERATE_ALL_CASES(NEXUS_DISPATCH_CASE, slab_ptr_, deallocate(ptr))
      default:
        return;
    }
  }

  [[nodiscard]] bool empty() const noexcept {
    if (slab_ptr_ == nullptr) return true;
    switch (class_idx_) {
      NEXUS_GENERATE_ALL_CASES(NEXUS_DISPATCH_CASE, slab_ptr_, empty())
      default:
        return true;
    }
  }

  [[nodiscard]] bool full() const noexcept {
    if (slab_ptr_ == nullptr) return true;
    switch (class_idx_) {
      NEXUS_GENERATE_ALL_CASES(NEXUS_DISPATCH_CASE, slab_ptr_, full())
      default:
        return true;
    }
  }

  [[nodiscard]] bool contains(const void* ptr) const noexcept {
    if (slab_ptr_ == nullptr) return false;
    switch (class_idx_) {
      NEXUS_GENERATE_ALL_CASES(NEXUS_DISPATCH_CASE, slab_ptr_, contains(ptr))
      default:
        return false;
    }
  }

  [[nodiscard]] void* base() const noexcept {
    if (slab_ptr_ == nullptr) return nullptr;
    switch (class_idx_) {
      NEXUS_GENERATE_ALL_CASES(NEXUS_DISPATCH_CASE, slab_ptr_, base())
      default:
        return nullptr;
    }
  }

 private:
  void create_slab(void* chunk) noexcept {
    switch (class_idx_) {
      NEXUS_GENERATE_ALL_CASES(NEXUS_CREATE_CASE, chunk)
      default:
        return;
    }
  }

  void destroy() noexcept {
    if (slab_ptr_ == nullptr) return;
    switch (class_idx_) {
      NEXUS_GENERATE_ALL_CASES(NEXUS_DESTROY_CASE, slab_ptr_)
      default:
        break;
    }
    slab_ptr_ = nullptr;
  }

  void* slab_ptr_{nullptr};
  size_t class_idx_{0};
};

#undef NEXUS_DISPATCH_CASE
#undef NEXUS_CREATE_CASE
#undef NEXUS_DESTROY_CASE
#undef NEXUS_GENERATE_ALL_CASES

}  // namespace nexusalloc::internal

namespace nexusalloc {
using internal::Slab;
}
