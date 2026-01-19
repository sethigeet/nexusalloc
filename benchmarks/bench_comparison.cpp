/**
 * @file bench_comparison.cpp
 * @brief Comprehensive benchmark comparison between NexusAlloc and other allocators
 *
 * This file benchmarks NexusAlloc against:
 * - Standard malloc (glibc/system allocator)
 * - jemalloc (if available)
 * - tcmalloc (if available)
 *
 * IMPORTANT: Linking jemalloc or tcmalloc in the same binary can cause
 * ~30-40% performance degradation for NexusAlloc due to:
 * - System call interposition (mmap, brk, etc.)
 * - Thread-local storage conflicts
 * - Memory management interference
 *
 * For accurate NexusAlloc vs malloc comparison, use bench_allocator.cpp
 * which doesn't link against other allocators.
 *
 * Benchmark scenarios:
 * 1. Single allocation/deallocation (various sizes)
 * 2. Batch allocation/deallocation
 * 3. Random size workload
 * 4. Producer-consumer pattern (LIFO/FIFO)
 * 5. Multi-threaded contention
 * 6. Fragmentation stress test
 * 7. Real-world simulation (mixed workload)
 */

#include <benchmark/benchmark.h>

#include <cstdlib>
#include <cstring>
#include <random>
#include <vector>

#include "nexusalloc/nexusalloc.hpp"

#ifdef NEXUSALLOC_HAS_JEMALLOC
// Define JEMALLOC_NO_DEMANGLE to keep je_* prefixed function names
#define JEMALLOC_NO_DEMANGLE
#include <jemalloc/jemalloc.h>
static inline void* jemalloc_alloc(size_t size) { return je_malloc(size); }
static inline void jemalloc_free(void* ptr) { je_free(ptr); }
#endif

#ifdef NEXUSALLOC_HAS_TCMALLOC
#include <gperftools/tcmalloc.h>
static inline void* tcmalloc_alloc(size_t size) { return tc_malloc(size); }
static inline void tcmalloc_free(void* ptr) { tc_free(ptr); }
#endif

using namespace nexusalloc;

namespace {

// Random number generator for benchmarks
thread_local std::mt19937 tls_rng{std::random_device{}()};

// ============================================================================
// Allocator Traits - Unified interface for different allocators
// ============================================================================

// Use __libc_malloc/__libc_free to bypass any allocator interposition
// from jemalloc or tcmalloc. These are internal glibc functions.
extern "C" {
extern void* __libc_malloc(size_t size);
extern void __libc_free(void* ptr);
}

// Base trait for glibc allocator (explicitly uses libc, not interposed malloc)
struct MallocAllocator {
  static void* alloc(size_t size) { return __libc_malloc(size); }
  static void dealloc(void* ptr, size_t /*size*/) { __libc_free(ptr); }
  static const char* name() { return "Malloc"; }
};

// NexusAlloc trait - requires size for deallocation
struct NexusAllocator {
  static void* alloc(size_t size) { return allocate(size); }
  static void dealloc(void* ptr, size_t size) { deallocate(ptr, size); }
  static const char* name() { return "NexusAlloc"; }
};

#ifdef NEXUSALLOC_HAS_JEMALLOC
struct JemallocAllocator {
  static void* alloc(size_t size) { return jemalloc_alloc(size); }
  static void dealloc(void* ptr, size_t /*size*/) { jemalloc_free(ptr); }
  static const char* name() { return "Jemalloc"; }
};
#endif

#ifdef NEXUSALLOC_HAS_TCMALLOC
struct TcmallocAllocator {
  static void* alloc(size_t size) { return tcmalloc_alloc(size); }
  static void dealloc(void* ptr, size_t /*size*/) { tcmalloc_free(ptr); }
  static const char* name() { return "Tcmalloc"; }
};
#endif

// ============================================================================
// Single Allocation/Deallocation Benchmark
// ============================================================================

// Dynamic size version (size comes from runtime, less optimization possible)
template <typename Allocator>
void BM_Single(benchmark::State& state) {
  const size_t size = static_cast<size_t>(state.range(0));
  for (auto _ : state) {
    void* ptr = Allocator::alloc(size);
    benchmark::DoNotOptimize(ptr);
    Allocator::dealloc(ptr, size);
  }
  state.SetBytesProcessed(state.iterations() * static_cast<int64_t>(size));
  state.SetLabel(std::to_string(size) + "B");
}

// Fixed size version (size is compile-time constant, allows full inlining)
// This matches the optimization level of bench_allocator.cpp
template <typename Allocator, size_t Size>
void BM_SingleFixed(benchmark::State& state) {
  for (auto _ : state) {
    void* ptr = Allocator::alloc(Size);
    benchmark::DoNotOptimize(ptr);
    Allocator::dealloc(ptr, Size);
  }
  state.SetBytesProcessed(state.iterations() * static_cast<int64_t>(Size));
}

// Fixed-size benchmarks (compile-time constant, best for comparison)
BENCHMARK(BM_SingleFixed<NexusAllocator, 64>)->Name("BM_NexusAlloc_Fixed64");
BENCHMARK(BM_SingleFixed<MallocAllocator, 64>)->Name("BM_Malloc_Fixed64");
#ifdef NEXUSALLOC_HAS_JEMALLOC
BENCHMARK(BM_SingleFixed<JemallocAllocator, 64>)->Name("BM_Jemalloc_Fixed64");
#endif
#ifdef NEXUSALLOC_HAS_TCMALLOC
BENCHMARK(BM_SingleFixed<TcmallocAllocator, 64>)->Name("BM_Tcmalloc_Fixed64");
#endif

// Dynamic size benchmarks (for testing various sizes)
BENCHMARK(BM_Single<NexusAllocator>)
    ->Name("BM_NexusAlloc_Single")
    ->Args({16})
    ->Args({32})
    ->Args({64})
    ->Args({128})
    ->Args({256})
    ->Args({512})
    ->Args({1024})
    ->Args({4096});

BENCHMARK(BM_Single<MallocAllocator>)
    ->Name("BM_Malloc_Single")
    ->Args({16})
    ->Args({32})
    ->Args({64})
    ->Args({128})
    ->Args({256})
    ->Args({512})
    ->Args({1024})
    ->Args({4096});

#ifdef NEXUSALLOC_HAS_JEMALLOC
BENCHMARK(BM_Single<JemallocAllocator>)
    ->Name("BM_Jemalloc_Single")
    ->Args({16})
    ->Args({32})
    ->Args({64})
    ->Args({128})
    ->Args({256})
    ->Args({512})
    ->Args({1024})
    ->Args({4096});
#endif

#ifdef NEXUSALLOC_HAS_TCMALLOC
BENCHMARK(BM_Single<TcmallocAllocator>)
    ->Name("BM_Tcmalloc_Single")
    ->Args({16})
    ->Args({32})
    ->Args({64})
    ->Args({128})
    ->Args({256})
    ->Args({512})
    ->Args({1024})
    ->Args({4096});
#endif

// ============================================================================
// Batch Allocation/Deallocation Benchmark
// ============================================================================

template <typename Allocator>
void BM_Batch(benchmark::State& state) {
  const size_t batch_size = static_cast<size_t>(state.range(0));
  const size_t alloc_size = static_cast<size_t>(state.range(1));
  std::vector<void*> ptrs(batch_size);

  for (auto _ : state) {
    for (size_t i = 0; i < batch_size; ++i) {
      ptrs[i] = Allocator::alloc(alloc_size);
    }
    for (size_t i = 0; i < batch_size; ++i) {
      Allocator::dealloc(ptrs[i], alloc_size);
    }
  }
  state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(batch_size) * 2);
  state.SetBytesProcessed(state.iterations() * static_cast<int64_t>(batch_size * alloc_size));
}

BENCHMARK(BM_Batch<NexusAllocator>)
    ->Name("BM_NexusAlloc_Batch")
    ->Args({100, 16})
    ->Args({100, 64})
    ->Args({100, 256})
    ->Args({100, 1024})
    ->Args({1000, 64})
    ->Args({10000, 64});

BENCHMARK(BM_Batch<MallocAllocator>)
    ->Name("BM_Malloc_Batch")
    ->Args({100, 16})
    ->Args({100, 64})
    ->Args({100, 256})
    ->Args({100, 1024})
    ->Args({1000, 64})
    ->Args({10000, 64});

#ifdef NEXUSALLOC_HAS_JEMALLOC
BENCHMARK(BM_Batch<JemallocAllocator>)
    ->Name("BM_Jemalloc_Batch")
    ->Args({100, 16})
    ->Args({100, 64})
    ->Args({100, 256})
    ->Args({100, 1024})
    ->Args({1000, 64})
    ->Args({10000, 64});
#endif

#ifdef NEXUSALLOC_HAS_TCMALLOC
BENCHMARK(BM_Batch<TcmallocAllocator>)
    ->Name("BM_Tcmalloc_Batch")
    ->Args({100, 16})
    ->Args({100, 64})
    ->Args({100, 256})
    ->Args({100, 1024})
    ->Args({1000, 64})
    ->Args({10000, 64});
#endif

// ============================================================================
// Random Size Workload Benchmark
// ============================================================================

template <typename Allocator>
void BM_RandomSize(benchmark::State& state) {
  std::uniform_int_distribution<size_t> size_dist(16, 4096);
  const size_t batch_size = 100;
  std::vector<void*> ptrs(batch_size);
  std::vector<size_t> sizes(batch_size);

  for (auto _ : state) {
    for (size_t i = 0; i < batch_size; ++i) {
      sizes[i] = size_dist(tls_rng);
      ptrs[i] = Allocator::alloc(sizes[i]);
    }
    for (size_t i = 0; i < batch_size; ++i) {
      Allocator::dealloc(ptrs[i], sizes[i]);
    }
  }
  state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(batch_size) * 2);
}

BENCHMARK(BM_RandomSize<NexusAllocator>)->Name("BM_NexusAlloc_RandomSize");
BENCHMARK(BM_RandomSize<MallocAllocator>)->Name("BM_Malloc_RandomSize");
#ifdef NEXUSALLOC_HAS_JEMALLOC
BENCHMARK(BM_RandomSize<JemallocAllocator>)->Name("BM_Jemalloc_RandomSize");
#endif
#ifdef NEXUSALLOC_HAS_TCMALLOC
BENCHMARK(BM_RandomSize<TcmallocAllocator>)->Name("BM_Tcmalloc_RandomSize");
#endif

// ============================================================================
// LIFO Pattern Benchmark (Stack-like usage)
// ============================================================================

template <typename Allocator>
void BM_LIFO(benchmark::State& state) {
  const size_t depth = static_cast<size_t>(state.range(0));
  const size_t alloc_size = 64;
  std::vector<void*> stack(depth);

  for (auto _ : state) {
    // Push phase
    for (size_t i = 0; i < depth; ++i) {
      stack[i] = Allocator::alloc(alloc_size);
    }
    // Pop phase (LIFO order)
    for (size_t i = depth; i > 0; --i) {
      Allocator::dealloc(stack[i - 1], alloc_size);
    }
  }
  state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(depth) * 2);
}

BENCHMARK(BM_LIFO<NexusAllocator>)->Name("BM_NexusAlloc_LIFO")->Range(8, 1024);
BENCHMARK(BM_LIFO<MallocAllocator>)->Name("BM_Malloc_LIFO")->Range(8, 1024);
#ifdef NEXUSALLOC_HAS_JEMALLOC
BENCHMARK(BM_LIFO<JemallocAllocator>)->Name("BM_Jemalloc_LIFO")->Range(8, 1024);
#endif
#ifdef NEXUSALLOC_HAS_TCMALLOC
BENCHMARK(BM_LIFO<TcmallocAllocator>)->Name("BM_Tcmalloc_LIFO")->Range(8, 1024);
#endif

// ============================================================================
// FIFO Pattern Benchmark (Queue-like usage)
// ============================================================================

template <typename Allocator>
void BM_FIFO(benchmark::State& state) {
  const size_t depth = static_cast<size_t>(state.range(0));
  const size_t alloc_size = 64;
  std::vector<void*> queue(depth);

  for (auto _ : state) {
    // Enqueue phase
    for (size_t i = 0; i < depth; ++i) {
      queue[i] = Allocator::alloc(alloc_size);
    }
    // Dequeue phase (FIFO order)
    for (size_t i = 0; i < depth; ++i) {
      Allocator::dealloc(queue[i], alloc_size);
    }
  }
  state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(depth) * 2);
}

BENCHMARK(BM_FIFO<NexusAllocator>)->Name("BM_NexusAlloc_FIFO")->Range(8, 1024);
BENCHMARK(BM_FIFO<MallocAllocator>)->Name("BM_Malloc_FIFO")->Range(8, 1024);
#ifdef NEXUSALLOC_HAS_JEMALLOC
BENCHMARK(BM_FIFO<JemallocAllocator>)->Name("BM_Jemalloc_FIFO")->Range(8, 1024);
#endif
#ifdef NEXUSALLOC_HAS_TCMALLOC
BENCHMARK(BM_FIFO<TcmallocAllocator>)->Name("BM_Tcmalloc_FIFO")->Range(8, 1024);
#endif

// ============================================================================
// Interleaved Allocation/Deallocation Benchmark
// ============================================================================

template <typename Allocator>
void BM_Interleaved(benchmark::State& state) {
  const size_t count = static_cast<size_t>(state.range(0));
  const size_t alloc_size = 64;
  std::vector<void*> ptrs(count);

  for (auto _ : state) {
    // Allocate half
    for (size_t i = 0; i < count / 2; ++i) {
      ptrs[i] = Allocator::alloc(alloc_size);
    }
    // Allocate and free interleaved
    for (size_t i = count / 2; i < count; ++i) {
      ptrs[i] = Allocator::alloc(alloc_size);
      Allocator::dealloc(ptrs[i - count / 2], alloc_size);
    }
    // Free remaining
    for (size_t i = count / 2; i < count; ++i) {
      Allocator::dealloc(ptrs[i], alloc_size);
    }
  }
  state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(count) * 2);
}

BENCHMARK(BM_Interleaved<NexusAllocator>)->Name("BM_NexusAlloc_Interleaved")->Range(100, 10000);
BENCHMARK(BM_Interleaved<MallocAllocator>)->Name("BM_Malloc_Interleaved")->Range(100, 10000);
#ifdef NEXUSALLOC_HAS_JEMALLOC
BENCHMARK(BM_Interleaved<JemallocAllocator>)->Name("BM_Jemalloc_Interleaved")->Range(100, 10000);
#endif
#ifdef NEXUSALLOC_HAS_TCMALLOC
BENCHMARK(BM_Interleaved<TcmallocAllocator>)->Name("BM_Tcmalloc_Interleaved")->Range(100, 10000);
#endif

// ============================================================================
// Multi-threaded Benchmark
// ============================================================================

template <typename Allocator>
void BM_MultiThreaded(benchmark::State& state) {
  const size_t alloc_size = 64;
  for (auto _ : state) {
    void* ptr = Allocator::alloc(alloc_size);
    benchmark::DoNotOptimize(ptr);
    Allocator::dealloc(ptr, alloc_size);
  }
}

BENCHMARK(BM_MultiThreaded<NexusAllocator>)
    ->Name("BM_NexusAlloc_MultiThreaded")
    ->Threads(1)
    ->Threads(2)
    ->Threads(4)
    ->Threads(8)
    ->Threads(16);

BENCHMARK(BM_MultiThreaded<MallocAllocator>)
    ->Name("BM_Malloc_MultiThreaded")
    ->Threads(1)
    ->Threads(2)
    ->Threads(4)
    ->Threads(8)
    ->Threads(16);

#ifdef NEXUSALLOC_HAS_JEMALLOC
BENCHMARK(BM_MultiThreaded<JemallocAllocator>)
    ->Name("BM_Jemalloc_MultiThreaded")
    ->Threads(1)
    ->Threads(2)
    ->Threads(4)
    ->Threads(8)
    ->Threads(16);
#endif

#ifdef NEXUSALLOC_HAS_TCMALLOC
BENCHMARK(BM_MultiThreaded<TcmallocAllocator>)
    ->Name("BM_Tcmalloc_MultiThreaded")
    ->Threads(1)
    ->Threads(2)
    ->Threads(4)
    ->Threads(8)
    ->Threads(16);
#endif

// ============================================================================
// Fragmentation Stress Test
// ============================================================================

template <typename Allocator>
void BM_Fragmentation(benchmark::State& state) {
  const size_t num_allocs = 1000;
  std::vector<void*> ptrs(num_allocs);
  std::vector<size_t> sizes(num_allocs);
  std::uniform_int_distribution<size_t> size_dist(16, 1024);

  for (auto _ : state) {
    // Phase 1: Allocate all
    for (size_t i = 0; i < num_allocs; ++i) {
      sizes[i] = size_dist(tls_rng);
      ptrs[i] = Allocator::alloc(sizes[i]);
    }
    // Phase 2: Free every other allocation
    for (size_t i = 0; i < num_allocs; i += 2) {
      Allocator::dealloc(ptrs[i], sizes[i]);
      ptrs[i] = nullptr;
    }
    // Phase 3: Reallocate in the holes
    for (size_t i = 0; i < num_allocs; i += 2) {
      sizes[i] = size_dist(tls_rng);
      ptrs[i] = Allocator::alloc(sizes[i]);
    }
    // Cleanup
    for (size_t i = 0; i < num_allocs; ++i) {
      if (ptrs[i]) Allocator::dealloc(ptrs[i], sizes[i]);
    }
  }
  state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(num_allocs) * 3);
}

BENCHMARK(BM_Fragmentation<NexusAllocator>)->Name("BM_NexusAlloc_Fragmentation");
BENCHMARK(BM_Fragmentation<MallocAllocator>)->Name("BM_Malloc_Fragmentation");
#ifdef NEXUSALLOC_HAS_JEMALLOC
BENCHMARK(BM_Fragmentation<JemallocAllocator>)->Name("BM_Jemalloc_Fragmentation");
#endif
#ifdef NEXUSALLOC_HAS_TCMALLOC
BENCHMARK(BM_Fragmentation<TcmallocAllocator>)->Name("BM_Tcmalloc_Fragmentation");
#endif

// ============================================================================
// Mixed Workload Simulation (Real-world scenario)
// ============================================================================

template <typename Allocator>
void BM_MixedWorkload(benchmark::State& state) {
  const size_t working_set = 500;
  std::vector<void*> live_ptrs;
  std::vector<size_t> live_sizes;
  live_ptrs.reserve(working_set);
  live_sizes.reserve(working_set);

  std::uniform_int_distribution<size_t> size_dist(16, 2048);
  std::uniform_real_distribution<double> action_dist(0.0, 1.0);

  for (auto _ : state) {
    // Perform a mix of allocations and deallocations
    for (size_t i = 0; i < 100; ++i) {
      double action = action_dist(tls_rng);

      if (live_ptrs.size() < working_set / 2 || action < 0.6) {
        // Allocate
        size_t size = size_dist(tls_rng);
        void* ptr = Allocator::alloc(size);
        benchmark::DoNotOptimize(ptr);
        live_ptrs.push_back(ptr);
        live_sizes.push_back(size);
      } else if (!live_ptrs.empty()) {
        // Deallocate random existing allocation
        std::uniform_int_distribution<size_t> idx_dist(0, live_ptrs.size() - 1);
        size_t idx = idx_dist(tls_rng);
        Allocator::dealloc(live_ptrs[idx], live_sizes[idx]);
        live_ptrs[idx] = live_ptrs.back();
        live_sizes[idx] = live_sizes.back();
        live_ptrs.pop_back();
        live_sizes.pop_back();
      }
    }
  }

  // Cleanup
  for (size_t i = 0; i < live_ptrs.size(); ++i) {
    Allocator::dealloc(live_ptrs[i], live_sizes[i]);
  }

  state.SetItemsProcessed(state.iterations() * 100);
}

BENCHMARK(BM_MixedWorkload<NexusAllocator>)->Name("BM_NexusAlloc_MixedWorkload");
BENCHMARK(BM_MixedWorkload<MallocAllocator>)->Name("BM_Malloc_MixedWorkload");
#ifdef NEXUSALLOC_HAS_JEMALLOC
BENCHMARK(BM_MixedWorkload<JemallocAllocator>)->Name("BM_Jemalloc_MixedWorkload");
#endif
#ifdef NEXUSALLOC_HAS_TCMALLOC
BENCHMARK(BM_MixedWorkload<TcmallocAllocator>)->Name("BM_Tcmalloc_MixedWorkload");
#endif

// ============================================================================
// Latency Distribution Benchmark
// ============================================================================

template <typename Allocator>
void BM_Latency(benchmark::State& state) {
  const size_t size = static_cast<size_t>(state.range(0));

  for (auto _ : state) {
    void* ptr = Allocator::alloc(size);
    benchmark::DoNotOptimize(ptr);
    Allocator::dealloc(ptr, size);
  }

  state.SetBytesProcessed(state.iterations() * static_cast<int64_t>(size));
}

BENCHMARK(BM_Latency<NexusAllocator>)
    ->Name("BM_NexusAlloc_Latency")
    ->Args({64})
    ->Repetitions(10)
    ->ReportAggregatesOnly();

BENCHMARK(BM_Latency<MallocAllocator>)
    ->Name("BM_Malloc_Latency")
    ->Args({64})
    ->Repetitions(10)
    ->ReportAggregatesOnly();

#ifdef NEXUSALLOC_HAS_JEMALLOC
BENCHMARK(BM_Latency<JemallocAllocator>)
    ->Name("BM_Jemalloc_Latency")
    ->Args({64})
    ->Repetitions(10)
    ->ReportAggregatesOnly();
#endif

#ifdef NEXUSALLOC_HAS_TCMALLOC
BENCHMARK(BM_Latency<TcmallocAllocator>)
    ->Name("BM_Tcmalloc_Latency")
    ->Args({64})
    ->Repetitions(10)
    ->ReportAggregatesOnly();
#endif

// ============================================================================
// Throughput Benchmark (Sustained allocation rate)
// ============================================================================

template <typename Allocator>
void BM_Throughput(benchmark::State& state) {
  const size_t alloc_size = static_cast<size_t>(state.range(0));
  const size_t num_ops = 10000;
  std::vector<void*> ptrs(num_ops);
  std::vector<size_t> sizes(num_ops, alloc_size);

  for (auto _ : state) {
    // Allocate all
    for (size_t i = 0; i < num_ops; ++i) {
      ptrs[i] = Allocator::alloc(alloc_size);
      benchmark::DoNotOptimize(ptrs[i]);
    }
    // Deallocate all
    for (size_t i = 0; i < num_ops; ++i) {
      Allocator::dealloc(ptrs[i], alloc_size);
    }
  }

  state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(num_ops) * 2);
  state.SetBytesProcessed(state.iterations() * static_cast<int64_t>(num_ops * alloc_size));
}

BENCHMARK(BM_Throughput<NexusAllocator>)
    ->Name("BM_NexusAlloc_Throughput")
    ->Args({64})
    ->Args({256})
    ->Args({1024});

BENCHMARK(BM_Throughput<MallocAllocator>)
    ->Name("BM_Malloc_Throughput")
    ->Args({64})
    ->Args({256})
    ->Args({1024});

#ifdef NEXUSALLOC_HAS_JEMALLOC
BENCHMARK(BM_Throughput<JemallocAllocator>)
    ->Name("BM_Jemalloc_Throughput")
    ->Args({64})
    ->Args({256})
    ->Args({1024});
#endif

#ifdef NEXUSALLOC_HAS_TCMALLOC
BENCHMARK(BM_Throughput<TcmallocAllocator>)
    ->Name("BM_Tcmalloc_Throughput")
    ->Args({64})
    ->Args({256})
    ->Args({1024});
#endif

}  // namespace

BENCHMARK_MAIN();
