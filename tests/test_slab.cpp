#include <gtest/gtest.h>

#include <set>

#include "nexusalloc/hugepage_provider.hpp"
#include "nexusalloc/slab.hpp"

using namespace nexusalloc;

class SlabTest : public ::testing::Test {
 protected:
  void* chunk_{nullptr};

  void SetUp() override {
    chunk_ = HugepageProvider::allocate_chunk();
    ASSERT_NE(chunk_, nullptr) << "Failed to allocate chunk for test";
  }

  void TearDown() override {
    if (chunk_) {
      HugepageProvider::deallocate_chunk(chunk_);
    }
  }
};

TEST_F(SlabTest, BasicAllocation) {
  Slab<64> slab(chunk_);
  chunk_ = nullptr;  // Slab now owns the chunk

  // Initially: no allocations made, but blocks are available
  EXPECT_TRUE(slab.empty());   // No allocations outstanding
  EXPECT_FALSE(slab.full());   // Has free blocks
  EXPECT_GT(slab.free_blocks(), 0);  // Has available blocks

  void* ptr = slab.allocate();
  ASSERT_NE(ptr, nullptr);
  EXPECT_EQ(slab.used_blocks(), 1);
  EXPECT_FALSE(slab.empty());  // Now has allocation outstanding
}

TEST_F(SlabTest, AllocationAlignment) {
  Slab<64> slab(chunk_);
  chunk_ = nullptr;

  void* ptr = slab.allocate();
  ASSERT_NE(ptr, nullptr);

  // Check 16-byte alignment
  uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);
  EXPECT_EQ(addr % 16, 0);
}

TEST_F(SlabTest, Deallocation) {
  Slab<64> slab(chunk_);
  chunk_ = nullptr;

  void* ptr = slab.allocate();
  EXPECT_EQ(slab.used_blocks(), 1);

  slab.deallocate(ptr);
  EXPECT_EQ(slab.used_blocks(), 0);
  EXPECT_TRUE(slab.empty());
}

TEST_F(SlabTest, Contains) {
  Slab<64> slab(chunk_);
  void* base = chunk_;
  chunk_ = nullptr;

  void* ptr = slab.allocate();
  EXPECT_TRUE(slab.contains(ptr));
  EXPECT_TRUE(slab.contains(base));

  // Pointer outside the slab
  int stack_var = 0;
  EXPECT_FALSE(slab.contains(&stack_var));
}

TEST_F(SlabTest, MultipleAllocations) {
  Slab<64> slab(chunk_);
  chunk_ = nullptr;

  std::set<void*> ptrs;

  // Allocate multiple blocks
  for (int i = 0; i < 100; ++i) {
    void* ptr = slab.allocate();
    ASSERT_NE(ptr, nullptr);
    // Each pointer should be unique
    EXPECT_EQ(ptrs.count(ptr), 0);
    ptrs.insert(ptr);
  }

  EXPECT_EQ(slab.used_blocks(), 100);

  // Deallocate all
  for (void* ptr : ptrs) {
    slab.deallocate(ptr);
  }

  EXPECT_TRUE(slab.empty());
}

TEST_F(SlabTest, ReuseAfterDeallocation) {
  Slab<64> slab(chunk_);
  chunk_ = nullptr;

  void* ptr1 = slab.allocate();
  slab.deallocate(ptr1);

  void* ptr2 = slab.allocate();
  // Should reuse the same block (LIFO free list)
  EXPECT_EQ(ptr1, ptr2);
}

TEST_F(SlabTest, OccupancyTracking) {
  Slab<64> slab(chunk_);
  chunk_ = nullptr;

  const auto& occupancy = slab.occupancy();
  EXPECT_TRUE(occupancy.none());

  void* ptr = slab.allocate();
  EXPECT_EQ(occupancy.count(), 1);

  slab.deallocate(ptr);
  EXPECT_TRUE(occupancy.none());
}
