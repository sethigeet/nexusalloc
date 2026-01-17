#include <gtest/gtest.h>

#include <thread>
#include <vector>

#include "nexusalloc/atomic_stack.hpp"
#include "nexusalloc/hugepage_provider.hpp"

using namespace nexusalloc;

TEST(AtomicStackTest, InitiallyEmpty) {
  AtomicStack stack;
  EXPECT_TRUE(stack.empty());
  EXPECT_EQ(stack.pop(), nullptr);
}

TEST(AtomicStackTest, PushPop) {
  AtomicStack stack;

  void* chunk = HugepageProvider::allocate_chunk();
  ASSERT_NE(chunk, nullptr);

  stack.push(chunk);
  EXPECT_FALSE(stack.empty());

  void* popped = stack.pop();
  EXPECT_EQ(popped, chunk);
  EXPECT_TRUE(stack.empty());

  HugepageProvider::deallocate_chunk(chunk);
}

TEST(AtomicStackTest, LIFO) {
  AtomicStack stack;

  std::vector<void*> chunks;
  for (int i = 0; i < 5; ++i) {
    void* chunk = HugepageProvider::allocate_chunk();
    ASSERT_NE(chunk, nullptr);
    chunks.push_back(chunk);
    stack.push(chunk);
  }

  // Pop should return in reverse order (LIFO)
  for (int i = 4; i >= 0; --i) {
    void* popped = stack.pop();
    EXPECT_EQ(popped, chunks[i]);
  }

  EXPECT_TRUE(stack.empty());

  for (void* chunk : chunks) {
    HugepageProvider::deallocate_chunk(chunk);
  }
}

TEST(AtomicStackTest, NullPush) {
  AtomicStack stack;

  // Pushing null should be a no-op
  stack.push(nullptr);
  EXPECT_TRUE(stack.empty());
}

TEST(AtomicStackTest, ApproximateSize) {
  AtomicStack stack;

  EXPECT_EQ(stack.approximate_size(), 0);

  std::vector<void*> chunks;
  for (int i = 0; i < 3; ++i) {
    void* chunk = HugepageProvider::allocate_chunk();
    ASSERT_NE(chunk, nullptr);
    chunks.push_back(chunk);
    stack.push(chunk);
  }

  EXPECT_EQ(stack.approximate_size(), 3);

  // Pop all and collect for deallocation
  std::vector<void*> popped;
  while (void* p = stack.pop()) {
    popped.push_back(p);
  }

  EXPECT_EQ(popped.size(), 3);

  // Deallocate the popped chunks (same memory as original chunks)
  for (void* chunk : popped) {
    HugepageProvider::deallocate_chunk(chunk);
  }
}

TEST(AtomicStackTest, ConcurrentPush) {
  AtomicStack stack;
  constexpr int kNumThreads = 4;
  constexpr int kPushesPerThread = 10;

  std::vector<void*> all_chunks;
  std::mutex chunks_mutex;

  auto push_work = [&]() {
    for (int i = 0; i < kPushesPerThread; ++i) {
      void* chunk = HugepageProvider::allocate_chunk();
      if (chunk) {
        {
          std::lock_guard<std::mutex> lock(chunks_mutex);
          all_chunks.push_back(chunk);
        }
        stack.push(chunk);
      }
    }
  };

  std::vector<std::thread> threads;
  for (int i = 0; i < kNumThreads; ++i) {
    threads.emplace_back(push_work);
  }

  for (auto& t : threads) {
    t.join();
  }

  // All chunks should be in the stack
  size_t count = 0;
  while (stack.pop() != nullptr) {
    ++count;
  }

  EXPECT_EQ(count, all_chunks.size());

  for (void* chunk : all_chunks) {
    HugepageProvider::deallocate_chunk(chunk);
  }
}

TEST(AtomicStackTest, ConcurrentPushPop) {
  AtomicStack stack;
  constexpr int kNumThreads = 4;
  constexpr int kOpsPerThread = 50;

  std::atomic<int> push_count{0};
  std::atomic<int> pop_count{0};
  std::vector<void*> popped_chunks;
  std::mutex popped_chunks_mutex;

  auto worker = [&](int thread_id) {
    for (int i = 0; i < kOpsPerThread; ++i) {
      if ((i + thread_id) % 2 == 0) {
        void* chunk = HugepageProvider::allocate_chunk();
        if (chunk) {
          stack.push(chunk);
          push_count.fetch_add(1);
        }
      } else {
        void* chunk = stack.pop();
        if (chunk) {
          pop_count.fetch_add(1);

          std::lock_guard<std::mutex> lock(popped_chunks_mutex);
          popped_chunks.push_back(chunk);
        }
      }
    }
  };

  std::vector<std::thread> threads;
  for (int i = 0; i < kNumThreads; ++i) {
    threads.emplace_back(worker, i);
  }

  for (auto& t : threads) {
    t.join();
  }

  // Clean up remaining items in stack
  while (void* chunk = stack.pop()) {
    popped_chunks.push_back(chunk);
  }

  // Clean up all chunks
  for (void* chunk : popped_chunks) {
    HugepageProvider::deallocate_chunk(chunk);
  }

  // We shouldn't have popped more than we pushed
  EXPECT_GE(push_count.load(), pop_count.load());
}
