#include <gtest/gtest.h>

#include <atomic>
#include <random>
#include <thread>
#include <vector>

#include "nexusalloc/nexusalloc.hpp"

using namespace nexusalloc;

// Stress test with many allocations/deallocations
TEST(StressTest, SingleThreadManyAllocations) {
  constexpr int kNumIterations = 10000;
  std::vector<std::pair<void*, size_t>> allocations;
  allocations.reserve(kNumIterations);

  std::mt19937 rng(42);
  std::uniform_int_distribution<size_t> size_dist(1, 1024);

  for (int i = 0; i < kNumIterations; ++i) {
    size_t size = size_dist(rng);
    void* ptr = allocate(size);
    ASSERT_NE(ptr, nullptr) << "Allocation failed at iteration " << i;
    allocations.push_back({ptr, size});

    // Write to verify memory is usable
    memset(ptr, 0xAB, size);
  }

  // Deallocate all
  for (auto& [ptr, size] : allocations) {
    deallocate(ptr, size);
  }
}

// Stress test with interleaved alloc/dealloc
TEST(StressTest, InterleavedAllocDealloc) {
  constexpr int kNumIterations = 10000;
  std::vector<std::pair<void*, size_t>> active;

  std::mt19937 rng(123);
  std::uniform_int_distribution<size_t> size_dist(16, 512);
  std::uniform_int_distribution<int> action_dist(0, 2);

  for (int i = 0; i < kNumIterations; ++i) {
    int action = action_dist(rng);

    if (action < 2 || active.empty()) {
      // Allocate (2/3 chance, or if no active allocations)
      size_t size = size_dist(rng);
      void* ptr = allocate(size);
      ASSERT_NE(ptr, nullptr);
      active.push_back({ptr, size});
    } else {
      // Deallocate random active allocation
      std::uniform_int_distribution<size_t> idx_dist(0, active.size() - 1);
      size_t idx = idx_dist(rng);
      auto [ptr, size] = active[idx];
      deallocate(ptr, size);
      active[idx] = active.back();
      active.pop_back();
    }
  }

  // Clean up remaining
  for (auto& [ptr, size] : active) {
    deallocate(ptr, size);
  }
}

// Multi-threaded stress test
TEST(StressTest, MultiThreaded) {
  constexpr int kNumThreads = 4;
  constexpr int kOpsPerThread = 1000;

  std::atomic<int> total_allocs{0};
  std::atomic<int> total_deallocs{0};

  auto worker = [&](int thread_id) {
    std::mt19937 rng(thread_id * 1000);
    std::uniform_int_distribution<size_t> size_dist(16, 256);

    std::vector<std::pair<void*, size_t>> my_allocations;
    my_allocations.reserve(kOpsPerThread);

    for (int i = 0; i < kOpsPerThread; ++i) {
      size_t size = size_dist(rng);
      void* ptr = allocate(size);
      if (ptr != nullptr) {
        total_allocs.fetch_add(1);
        my_allocations.push_back({ptr, size});

        // Write to verify memory
        memset(ptr, static_cast<int>(thread_id), size);
      }

      // Occasionally deallocate some
      if (!my_allocations.empty() && (i % 3 == 0)) {
        auto [dptr, dsize] = my_allocations.back();
        my_allocations.pop_back();
        deallocate(dptr, dsize);
        total_deallocs.fetch_add(1);
      }
    }

    // Clean up remaining
    for (auto& [ptr, size] : my_allocations) {
      deallocate(ptr, size);
      total_deallocs.fetch_add(1);
    }
  };

  std::vector<std::thread> threads;
  for (int i = 0; i < kNumThreads; ++i) {
    threads.emplace_back(worker, i);
  }

  for (auto& t : threads) {
    t.join();
  }

  // All allocations should have been deallocated
  EXPECT_EQ(total_allocs.load(), total_deallocs.load());
}

// Test STL container under stress
TEST(StressTest, STLContainerStress) {
  std::vector<int, NexusAllocator<int>> vec;

  // Many push_back operations
  for (int i = 0; i < 10000; ++i) {
    vec.push_back(i);
  }

  EXPECT_EQ(vec.size(), 10000);

  // Verify contents
  for (int i = 0; i < 10000; ++i) {
    EXPECT_EQ(vec[i], i);
  }

  // Erase from middle
  vec.erase(vec.begin() + 5000, vec.begin() + 7000);
  EXPECT_EQ(vec.size(), 8000);

  // Clear
  vec.clear();
  EXPECT_EQ(vec.size(), 0);
}

// Test various size classes under load
TEST(StressTest, AllSizeClasses) {
  const auto& sizes = SizeClass::sizes();

  for (size_t block_size : sizes) {
    std::vector<void*> ptrs;

    // Allocate many blocks of this size class
    for (int i = 0; i < 100; ++i) {
      void* ptr = allocate(block_size);
      ASSERT_NE(ptr, nullptr) << "Failed for size " << block_size;
      ptrs.push_back(ptr);
    }

    // Deallocate all
    for (void* ptr : ptrs) {
      deallocate(ptr, block_size);
    }
  }
}
