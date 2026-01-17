include(FetchContent)

# Fetch Google Benchmark
FetchContent_Declare(
    googlebenchmark
    GIT_REPOSITORY https://github.com/google/benchmark.git
    GIT_TAG v1.8.3
)
set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googlebenchmark)

add_executable(bench_allocator
    bench_allocator.cpp
)

target_link_libraries(bench_allocator PRIVATE
    nexusalloc
    benchmark::benchmark
    pthread
)
