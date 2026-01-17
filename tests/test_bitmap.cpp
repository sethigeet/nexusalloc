#include <gtest/gtest.h>

#include "nexusalloc/internal/bitmap.hpp"

using namespace nexusalloc::internal;

TEST(BitmapTest, InitiallyEmpty) {
  Bitmap<64> bm;
  EXPECT_TRUE(bm.none());
  EXPECT_FALSE(bm.all());
  EXPECT_EQ(bm.count(), 0);
}

TEST(BitmapTest, SetAndTest) {
  Bitmap<64> bm;

  bm.set(0);
  EXPECT_TRUE(bm.test(0));
  EXPECT_FALSE(bm.test(1));
  EXPECT_EQ(bm.count(), 1);

  bm.set(63);
  EXPECT_TRUE(bm.test(63));
  EXPECT_EQ(bm.count(), 2);
}

TEST(BitmapTest, Clear) {
  Bitmap<64> bm;

  bm.set(10);
  EXPECT_TRUE(bm.test(10));

  bm.clear(10);
  EXPECT_FALSE(bm.test(10));
  EXPECT_EQ(bm.count(), 0);
}

TEST(BitmapTest, All) {
  Bitmap<64> bm;

  for (size_t i = 0; i < 64; ++i) {
    bm.set(i);
  }

  EXPECT_TRUE(bm.all());
  EXPECT_EQ(bm.count(), 64);
}

TEST(BitmapTest, FindFirstClear) {
  Bitmap<64> bm;

  EXPECT_EQ(bm.find_first_clear(), 0);

  bm.set(0);
  EXPECT_EQ(bm.find_first_clear(), 1);

  bm.set(1);
  bm.set(2);
  EXPECT_EQ(bm.find_first_clear(), 3);

  // Set all bits
  for (size_t i = 0; i < 64; ++i) {
    bm.set(i);
  }
  EXPECT_EQ(bm.find_first_clear(), 64);
}

TEST(BitmapTest, Reset) {
  Bitmap<64> bm;

  for (size_t i = 0; i < 32; ++i) {
    bm.set(i);
  }
  EXPECT_EQ(bm.count(), 32);

  bm.reset();
  EXPECT_TRUE(bm.none());
  EXPECT_EQ(bm.count(), 0);
}

TEST(BitmapTest, MultiWord) {
  // Test bitmap that spans multiple 64-bit words
  Bitmap<256> bm;

  bm.set(0);
  bm.set(64);
  bm.set(128);
  bm.set(192);

  EXPECT_TRUE(bm.test(0));
  EXPECT_TRUE(bm.test(64));
  EXPECT_TRUE(bm.test(128));
  EXPECT_TRUE(bm.test(192));
  EXPECT_EQ(bm.count(), 4);
}

TEST(BitmapTest, NonPowerOfTwo) {
  // Test bitmap with non-power-of-2 size
  Bitmap<100> bm;

  bm.set(99);
  EXPECT_TRUE(bm.test(99));

  // Fill all 100 bits
  for (size_t i = 0; i < 100; ++i) {
    bm.set(i);
  }
  EXPECT_TRUE(bm.all());
  EXPECT_EQ(bm.count(), 100);
}
