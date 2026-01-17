#include <gtest/gtest.h>

#include "nexusalloc/internal/size_class.hpp"

using namespace nexusalloc::internal;

TEST(SizeClassTest, SmallClassSizes) {
  // Check that size classes have expected values
  const auto& sizes = SizeClass::sizes();

  EXPECT_EQ(sizes[0], 16);
  EXPECT_EQ(sizes[1], 32);
  EXPECT_EQ(sizes[15], 256);  // Last small class
}

TEST(SizeClassTest, LargeClassSizes) {
  const auto& sizes = SizeClass::sizes();

  EXPECT_EQ(sizes[16], 512);
  EXPECT_EQ(sizes[17], 1024);
  EXPECT_EQ(sizes[23], 65536);  // Last large class
}

TEST(SizeClassTest, IndexForSmallSizes) {
  // 0 and 1 byte should map to class 0 (16 bytes)
  EXPECT_EQ(SizeClass::index(0), 0);
  EXPECT_EQ(SizeClass::index(1), 0);
  EXPECT_EQ(SizeClass::index(16), 0);

  // 17-32 bytes should map to class 1 (32 bytes)
  EXPECT_EQ(SizeClass::index(17), 1);
  EXPECT_EQ(SizeClass::index(32), 1);

  // 256 bytes should map to class 15
  EXPECT_EQ(SizeClass::index(256), 15);
}

TEST(SizeClassTest, IndexForLargeSizes) {
  // 257-512 should map to class 16 (512 bytes)
  EXPECT_EQ(SizeClass::index(257), 16);
  EXPECT_EQ(SizeClass::index(512), 16);

  // 65536 should map to class 23
  EXPECT_EQ(SizeClass::index(65536), 23);
}

TEST(SizeClassTest, TooLargeSize) {
  // Sizes > 65536 should return kNumClasses (indicating direct mmap)
  EXPECT_EQ(SizeClass::index(65537), SizeClass::kNumClasses);
  EXPECT_EQ(SizeClass::index(100000), SizeClass::kNumClasses);
}

TEST(SizeClassTest, IsLarge) {
  EXPECT_FALSE(SizeClass::is_large(1));
  EXPECT_FALSE(SizeClass::is_large(1000));
  EXPECT_FALSE(SizeClass::is_large(65536));
  EXPECT_TRUE(SizeClass::is_large(65537));
  EXPECT_TRUE(SizeClass::is_large(1000000));
}

TEST(SizeClassTest, BlockSize) {
  EXPECT_EQ(SizeClass::block_size(0), 16);
  EXPECT_EQ(SizeClass::block_size(15), 256);
  EXPECT_EQ(SizeClass::block_size(16), 512);
  EXPECT_EQ(SizeClass::block_size(23), 65536);

  // Invalid index
  EXPECT_EQ(SizeClass::block_size(24), 0);
  EXPECT_EQ(SizeClass::block_size(100), 0);
}

TEST(SizeClassTest, RoundTrip) {
  // For any size, the block size should be >= the original size
  for (size_t size = 1; size <= 65536; ++size) {
    size_t idx = SizeClass::index(size);
    ASSERT_LT(idx, SizeClass::kNumClasses);

    size_t block_sz = SizeClass::block_size(idx);
    EXPECT_GE(block_sz, size) << "Failed for size " << size;
  }
}
