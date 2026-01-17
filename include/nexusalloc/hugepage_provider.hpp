#pragma once

#include <sys/mman.h>
#include <unistd.h>

#include <atomic>
#include <cstddef>

namespace nexusalloc {

struct PageTraits {
  static constexpr size_t kHugePageSize = 2 * 1024 * 1024;  // 2MB
  static constexpr size_t kRegularPageSize = 4096;          // 4KB
  static constexpr size_t kChunkSize = kHugePageSize;       // Default chunk size
};

class HugepageProvider {
 public:
  HugepageProvider() = delete;

  [[nodiscard]] static void* allocate_chunk() noexcept {
    void* ptr = nullptr;

#ifdef NEXUSALLOC_USE_HUGEPAGES
    ptr = mmap(nullptr, PageTraits::kChunkSize, PROT_READ | PROT_WRITE,
               MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB | MAP_POPULATE, -1, 0);

    if (ptr == MAP_FAILED) {
      ptr = allocate_regular_chunk();
    }
#else
    ptr = allocate_regular_chunk();
#endif

    return ptr;
  }

  static void deallocate_chunk(void* ptr) noexcept {
    if (ptr != nullptr && ptr != MAP_FAILED) {
      munmap(ptr, PageTraits::kChunkSize);
    }
  }

  static bool lock_memory() noexcept {
    if (memory_locked_.load(std::memory_order_relaxed)) {
      return true;
    }

#ifdef MCL_CURRENT
    int result = mlockall(MCL_CURRENT | MCL_FUTURE);
    if (result == 0) {
      memory_locked_.store(true, std::memory_order_relaxed);
      return true;
    }
#endif
    return false;
  }

  [[nodiscard]] static bool is_memory_locked() noexcept {
    return memory_locked_.load(std::memory_order_relaxed);
  }

  [[nodiscard]] static constexpr size_t chunk_size() noexcept { return PageTraits::kChunkSize; }

 private:
  [[nodiscard]] static void* allocate_regular_chunk() noexcept {
    void* ptr = mmap(nullptr, PageTraits::kChunkSize, PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_ANONYMOUS | MAP_POPULATE, -1, 0);

    if (ptr == MAP_FAILED) {
      return nullptr;
    }
    return ptr;
  }

  static inline std::atomic<bool> memory_locked_{false};
};

}  // namespace nexusalloc
