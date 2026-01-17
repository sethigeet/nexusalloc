include(FetchContent)

# Fetch Google Test
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.14.0
)
FetchContent_MakeAvailable(googletest)

# Test executable
add_executable(nexusalloc_tests
    test_bitmap.cpp
    test_size_class.cpp
    test_slab.cpp
    test_atomic_stack.cpp
    test_thread_arena.cpp
)

target_link_libraries(nexusalloc_tests PRIVATE
    nexusalloc
    GTest::gtest_main
    pthread
)

include(GoogleTest)
gtest_discover_tests(nexusalloc_tests)
