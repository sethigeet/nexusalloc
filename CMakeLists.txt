cmake_minimum_required(VERSION 3.20)
project(nexusalloc VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Build options
option(NEXUSALLOC_USE_HUGEPAGES "Enable hugepage support" ON)

add_library(nexusalloc INTERFACE)
target_include_directories(nexusalloc INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
target_compile_features(nexusalloc INTERFACE cxx_std_20)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(nexusalloc INTERFACE
        -Wall -Wextra -Wpedantic
    )
endif()

if(NEXUSALLOC_USE_HUGEPAGES)
    target_compile_definitions(nexusalloc INTERFACE NEXUSALLOC_USE_HUGEPAGES=1)
endif()

# Link atomic library for 128-bit CAS operations (required for TaggedPtr)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_link_libraries(nexusalloc INTERFACE atomic)
endif()