#include <gtest/gtest.h>

#include <cstring>
#include <set>

#include "nexusalloc/thread_arena.hpp"

using namespace nexusalloc;

TEST(ThreadArenaTest, BasicAllocation) {
  void* ptr = ThreadArena::get().allocate(64);
  ASSERT_NE(ptr, nullptr);

  // Check alignment
  uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);
  EXPECT_EQ(addr % 16, 0);

  ThreadArena::get().deallocate(ptr, 64);
}

TEST(ThreadArenaTest, ZeroSize) {
  // Zero size should still allocate (minimum size)
  void* ptr = ThreadArena::get().allocate(0);
  EXPECT_NE(ptr, nullptr);
  ThreadArena::get().deallocate(ptr, 0);
}

TEST(ThreadArenaTest, VariousSizes) {
  std::vector<std::pair<void*, size_t>> allocations;

  // Test various size classes
  std::vector<size_t> sizes = {1, 16, 32, 64, 128, 256, 512, 1024, 4096, 65536};

  for (size_t size : sizes) {
    void* ptr = ThreadArena::get().allocate(size);
    ASSERT_NE(ptr, nullptr) << "Failed to allocate " << size << " bytes";
    allocations.push_back({ptr, size});
  }

  // Deallocate all
  for (auto& [ptr, size] : allocations) {
    ThreadArena::get().deallocate(ptr, size);
  }
}

TEST(ThreadArenaTest, LargeAllocation) {
  // Test allocation larger than max slab size (direct mmap)
  size_t large_size = 128 * 1024;  // 128KB
  void* ptr = ThreadArena::get().allocate(large_size);
  ASSERT_NE(ptr, nullptr);

  ThreadArena::get().deallocate(ptr, large_size);
}

TEST(ThreadArenaTest, UniquePOinters) {
  std::set<void*> ptrs;

  // Allocate many pointers, all should be unique
  for (int i = 0; i < 100; ++i) {
    void* ptr = ThreadArena::get().allocate(64);
    ASSERT_NE(ptr, nullptr);
    EXPECT_EQ(ptrs.count(ptr), 0) << "Duplicate pointer at iteration " << i;
    ptrs.insert(ptr);
  }

  // Deallocate all
  for (void* ptr : ptrs) {
    ThreadArena::get().deallocate(ptr, 64);
  }
}

TEST(ThreadArenaTest, NullDeallocation) {
  // Deallocating nullptr should be safe
  ThreadArena::get().deallocate(nullptr, 64);
}

TEST(ThreadArenaTest, CrossSizeClassDeallocation) {
  // Allocate multiple size classes, then deallocate in different order
  void* ptr16 = ThreadArena::get().allocate(16);
  void* ptr64 = ThreadArena::get().allocate(64);
  void* ptr256 = ThreadArena::get().allocate(256);
  void* ptr1024 = ThreadArena::get().allocate(1024);

  ASSERT_NE(ptr16, nullptr);
  ASSERT_NE(ptr64, nullptr);
  ASSERT_NE(ptr256, nullptr);
  ASSERT_NE(ptr1024, nullptr);

  // Deallocate in different order
  ThreadArena::get().deallocate(ptr256, 256);
  ThreadArena::get().deallocate(ptr16, 16);
  ThreadArena::get().deallocate(ptr1024, 1024);
  ThreadArena::get().deallocate(ptr64, 64);
}
