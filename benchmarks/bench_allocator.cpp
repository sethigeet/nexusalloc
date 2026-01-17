#include <benchmark/benchmark.h>

#include <cstdlib>
#include <vector>

#include "nexusalloc/nexusalloc.hpp"

using namespace nexusalloc;

// Benchmark small allocations (size class 0: 16 bytes)
static void BM_NexusAlloc_Small(benchmark::State& state) {
  for (auto _ : state) {
    void* ptr = allocate(16);
    benchmark::DoNotOptimize(ptr);
    deallocate(ptr, 16);
  }
}
BENCHMARK(BM_NexusAlloc_Small);

// Compare with malloc
static void BM_Malloc_Small(benchmark::State& state) {
  for (auto _ : state) {
    void* ptr = malloc(16);
    benchmark::DoNotOptimize(ptr);
    free(ptr);
  }
}
BENCHMARK(BM_Malloc_Small);

// Benchmark medium allocations (64 bytes)
static void BM_NexusAlloc_Medium(benchmark::State& state) {
  for (auto _ : state) {
    void* ptr = allocate(64);
    benchmark::DoNotOptimize(ptr);
    deallocate(ptr, 64);
  }
}
BENCHMARK(BM_NexusAlloc_Medium);

static void BM_Malloc_Medium(benchmark::State& state) {
  for (auto _ : state) {
    void* ptr = malloc(64);
    benchmark::DoNotOptimize(ptr);
    free(ptr);
  }
}
BENCHMARK(BM_Malloc_Medium);

// Benchmark large allocations (1024 bytes)
static void BM_NexusAlloc_Large(benchmark::State& state) {
  for (auto _ : state) {
    void* ptr = allocate(1024);
    benchmark::DoNotOptimize(ptr);
    deallocate(ptr, 1024);
  }
}
BENCHMARK(BM_NexusAlloc_Large);

static void BM_Malloc_Large(benchmark::State& state) {
  for (auto _ : state) {
    void* ptr = malloc(1024);
    benchmark::DoNotOptimize(ptr);
    free(ptr);
  }
}
BENCHMARK(BM_Malloc_Large);

// Benchmark allocation only (batch)
static void BM_NexusAlloc_BatchAlloc(benchmark::State& state) {
  const size_t batch_size = static_cast<size_t>(state.range(0));
  std::vector<void*> ptrs(batch_size);

  for (auto _ : state) {
    for (size_t i = 0; i < batch_size; ++i) {
      ptrs[i] = allocate(64);
    }
    for (size_t i = 0; i < batch_size; ++i) {
      deallocate(ptrs[i], 64);
    }
  }
  state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(batch_size));
}
BENCHMARK(BM_NexusAlloc_BatchAlloc)->Range(8, 1024);

static void BM_Malloc_BatchAlloc(benchmark::State& state) {
  const size_t batch_size = static_cast<size_t>(state.range(0));
  std::vector<void*> ptrs(batch_size);

  for (auto _ : state) {
    for (size_t i = 0; i < batch_size; ++i) {
      ptrs[i] = malloc(64);
    }
    for (size_t i = 0; i < batch_size; ++i) {
      free(ptrs[i]);
    }
  }
  state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(batch_size));
}
BENCHMARK(BM_Malloc_BatchAlloc)->Range(8, 1024);

// Benchmark various size classes
static void BM_NexusAlloc_SizeClasses(benchmark::State& state) {
  const size_t size = static_cast<size_t>(state.range(0));

  for (auto _ : state) {
    void* ptr = allocate(size);
    benchmark::DoNotOptimize(ptr);
    deallocate(ptr, size);
  }
  state.SetLabel(std::to_string(size) + " bytes");
}
BENCHMARK(BM_NexusAlloc_SizeClasses)
    ->Args({16})
    ->Args({32})
    ->Args({64})
    ->Args({128})
    ->Args({256})
    ->Args({512})
    ->Args({1024})
    ->Args({4096});

// Benchmark STL vector with NexusAllocator
static void BM_Vector_NexusAlloc(benchmark::State& state) {
  const size_t n = static_cast<size_t>(state.range(0));

  for (auto _ : state) {
    std::vector<int, NexusAllocator<int>> vec;
    for (size_t i = 0; i < n; ++i) {
      vec.push_back(static_cast<int>(i));
    }
    benchmark::DoNotOptimize(vec.data());
  }
  state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(n));
}
BENCHMARK(BM_Vector_NexusAlloc)->Range(8, 4096);

static void BM_Vector_StdAlloc(benchmark::State& state) {
  const size_t n = static_cast<size_t>(state.range(0));

  for (auto _ : state) {
    std::vector<int> vec;
    for (size_t i = 0; i < n; ++i) {
      vec.push_back(static_cast<int>(i));
    }
    benchmark::DoNotOptimize(vec.data());
  }
  state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(n));
}
BENCHMARK(BM_Vector_StdAlloc)->Range(8, 4096);

// Multi-threaded benchmark
static void BM_NexusAlloc_MultiThreaded(benchmark::State& state) {
  for (auto _ : state) {
    void* ptr = allocate(64);
    benchmark::DoNotOptimize(ptr);
    deallocate(ptr, 64);
  }
}
BENCHMARK(BM_NexusAlloc_MultiThreaded)->Threads(1)->Threads(2)->Threads(4)->Threads(8);

static void BM_Malloc_MultiThreaded(benchmark::State& state) {
  for (auto _ : state) {
    void* ptr = malloc(64);
    benchmark::DoNotOptimize(ptr);
    free(ptr);
  }
}
BENCHMARK(BM_Malloc_MultiThreaded)->Threads(1)->Threads(2)->Threads(4)->Threads(8);

BENCHMARK_MAIN();
