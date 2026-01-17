#pragma once

#include <cstddef>

#include "nexusalloc/hugepage_provider.hpp"
#include "nexusalloc/internal/alignment.hpp"
#include "nexusalloc/internal/bitmap.hpp"
#include "nexusalloc/internal/prefetch.hpp"

namespace nexusalloc {

namespace internal {

class SlabBase {
 public:
  virtual ~SlabBase() = default;
  [[nodiscard]] virtual void* allocate() noexcept = 0;
  virtual void deallocate(void* ptr) noexcept = 0;
  [[nodiscard]] virtual bool empty() const noexcept = 0;
  [[nodiscard]] virtual bool full() const noexcept = 0;
  [[nodiscard]] virtual bool contains(const void* ptr) const noexcept = 0;
  [[nodiscard]] virtual void* base() const noexcept = 0;
};
}  // namespace internal

template <size_t BlockSize>
class Slab : public internal::SlabBase {
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

  // Non-copyable, non-movable
  Slab(const Slab&) = delete;
  Slab& operator=(const Slab&) = delete;
  Slab(Slab&&) = delete;
  Slab& operator=(Slab&&) = delete;

  [[nodiscard]] void* allocate() noexcept override {
    if (free_head_ == nullptr) {
      return nullptr;
    }

    void* block = free_head_;
    void* next = *static_cast<void**>(block);
    if (next != nullptr) {
      internal::prefetch_read(next);  // prefetch the next block for bursty allocations
    }

    free_head_ = next;
    ++allocated_count_;

    size_t idx = block_index(block);
    occupancy_.set(idx);

    return block;
  }

  void deallocate(void* ptr) noexcept override {
    if (ptr == nullptr || !contains(ptr)) {
      return;
    }

    size_t idx = block_index(ptr);
    occupancy_.clear(idx);

    *static_cast<void**>(ptr) = free_head_;
    free_head_ = ptr;
    --allocated_count_;
  }

  [[nodiscard]] bool empty() const noexcept override { return allocated_count_ == 0; }

  [[nodiscard]] bool full() const noexcept override { return free_head_ == nullptr; }

  [[nodiscard]] size_t used_blocks() const noexcept { return allocated_count_; }

  [[nodiscard]] size_t free_blocks() const noexcept { return kBlocksPerSlab - allocated_count_; }

  [[nodiscard]] bool contains(const void* ptr) const noexcept override {
    const char* p = static_cast<const char*>(ptr);
    const char* base = static_cast<const char*>(base_);
    return p >= base && p < base + kChunkSize;
  }

  [[nodiscard]] void* base() const noexcept override { return base_; }

  [[nodiscard]] const internal::Bitmap<kBlocksPerSlab>& occupancy() const noexcept {
    return occupancy_;
  }

 private:
  [[nodiscard]] size_t block_index(const void* ptr) const noexcept {
    return (static_cast<const char*>(ptr) - static_cast<const char*>(base_)) / kBlockSize;
  }

  void* base_{nullptr};
  void* free_head_{nullptr};
  size_t allocated_count_{0};

  // Bitmap for tracking (1 bit per block)
  // Aligned to cache line to prevent false sharing
  alignas(internal::kCacheLineSize) internal::Bitmap<kBlocksPerSlab> occupancy_;
};

}  // namespace nexusalloc
