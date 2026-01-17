#include <gtest/gtest.h>

#include <map>
#include <string>
#include <vector>

#include "nexusalloc/allocator.hpp"

using namespace nexusalloc;

TEST(AllocatorTest, VectorOfInt) {
  std::vector<int, NexusAllocator<int>> vec;

  for (int i = 0; i < 100; ++i) {
    vec.push_back(i);
  }

  EXPECT_EQ(vec.size(), 100);

  for (int i = 0; i < 100; ++i) {
    EXPECT_EQ(vec[i], i);
  }
}

TEST(AllocatorTest, VectorReserve) {
  std::vector<int, NexusAllocator<int>> vec;
  vec.reserve(1000);

  EXPECT_GE(vec.capacity(), 1000);
  EXPECT_EQ(vec.size(), 0);

  for (int i = 0; i < 1000; ++i) {
    vec.push_back(i);
  }

  EXPECT_EQ(vec.size(), 1000);
}

TEST(AllocatorTest, VectorOfString) {
  std::vector<std::string, NexusAllocator<std::string>> vec;

  vec.push_back("hello");
  vec.push_back("world");
  vec.push_back("nexusalloc");

  EXPECT_EQ(vec[0], "hello");
  EXPECT_EQ(vec[1], "world");
  EXPECT_EQ(vec[2], "nexusalloc");
}

TEST(AllocatorTest, MapWithAllocator) {
  using AllocType = NexusAllocator<std::pair<const int, std::string>>;
  std::map<int, std::string, std::less<int>, AllocType> map;

  map[1] = "one";
  map[2] = "two";
  map[3] = "three";

  EXPECT_EQ(map.size(), 3);
  EXPECT_EQ(map[1], "one");
  EXPECT_EQ(map[2], "two");
  EXPECT_EQ(map[3], "three");
}

TEST(AllocatorTest, AllocatorEquality) {
  NexusAllocator<int> alloc1;
  NexusAllocator<int> alloc2;
  NexusAllocator<double> alloc3;

  // Allocators should always be equal since they are stateless
  EXPECT_TRUE(alloc1 == alloc2);
  EXPECT_FALSE(alloc1 != alloc2);
  EXPECT_TRUE(alloc1 == alloc3);
}

TEST(AllocatorTest, CopyConstruction) {
  NexusAllocator<int> alloc1;
  NexusAllocator<int> alloc2(alloc1);

  EXPECT_TRUE(alloc1 == alloc2);
}

TEST(AllocatorTest, RebindConstruction) {
  NexusAllocator<int> alloc1;
  NexusAllocator<double> alloc2(alloc1);

  // Should compile and work
  double* ptr = alloc2.allocate(1);
  EXPECT_NE(ptr, nullptr);
  alloc2.deallocate(ptr, 1);
}

TEST(AllocatorTest, AllocateZero) {
  NexusAllocator<int> alloc;

  // Allocating zero elements should return nullptr
  int* ptr = alloc.allocate(0);
  EXPECT_EQ(ptr, nullptr);
}

TEST(AllocatorTest, LargeAllocation) {
  NexusAllocator<char> alloc;

  // Allocate more than max slab size
  size_t size = 1024 * 1024;  // 1MB
  char* ptr = alloc.allocate(size);
  ASSERT_NE(ptr, nullptr);

  // Write to the memory
  for (size_t i = 0; i < size; i += 4096) {
    ptr[i] = 'x';
  }

  alloc.deallocate(ptr, size);
}

TEST(AllocatorTest, VectorClear) {
  std::vector<int, NexusAllocator<int>> vec;

  for (int i = 0; i < 100; ++i) {
    vec.push_back(i);
  }

  vec.clear();
  EXPECT_EQ(vec.size(), 0);

  // Should be able to reuse
  for (int i = 0; i < 50; ++i) {
    vec.push_back(i * 2);
  }
  EXPECT_EQ(vec.size(), 50);
}

TEST(AllocatorTest, VectorShrinkToFit) {
  std::vector<int, NexusAllocator<int>> vec;

  vec.reserve(1000);
  for (int i = 0; i < 10; ++i) {
    vec.push_back(i);
  }

  vec.shrink_to_fit();
  EXPECT_LE(vec.capacity(), 100);  // Should have shrunk
  EXPECT_EQ(vec.size(), 10);
}
