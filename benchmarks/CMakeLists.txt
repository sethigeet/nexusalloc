include(FetchContent)

FetchContent_Declare(
    googlebenchmark
    GIT_REPOSITORY https://github.com/google/benchmark.git
    GIT_TAG v1.8.3
)
set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googlebenchmark)

option(NEXUSALLOC_BENCH_JEMALLOC "Enable jemalloc benchmarks (will be fetched if not found)" OFF)
option(NEXUSALLOC_BENCH_TCMALLOC "Enable tcmalloc benchmarks (will be fetched if not found)" OFF)

if(NEXUSALLOC_BENCH_JEMALLOC)
    # First try to find system jemalloc
    find_library(JEMALLOC_LIBRARY
        NAMES jemalloc
        HINTS /usr/lib /usr/local/lib /opt/jemalloc/lib
    )
    find_path(JEMALLOC_INCLUDE_DIR
        NAMES jemalloc/jemalloc.h
        HINTS /usr/include /usr/local/include /opt/jemalloc/include
    )

    if(JEMALLOC_LIBRARY AND JEMALLOC_INCLUDE_DIR)
        message(STATUS "Found system jemalloc: ${JEMALLOC_LIBRARY}")
        set(HAVE_JEMALLOC TRUE)
        set(JEMALLOC_TARGET_NAME "jemalloc_system")
        add_library(${JEMALLOC_TARGET_NAME} INTERFACE)
        target_include_directories(${JEMALLOC_TARGET_NAME} INTERFACE ${JEMALLOC_INCLUDE_DIR})
        target_link_libraries(${JEMALLOC_TARGET_NAME} INTERFACE ${JEMALLOC_LIBRARY})
    else()
        message(STATUS "System jemalloc not found - fetching from source...")
        FetchContent_Declare(
            jemalloc
            GIT_REPOSITORY https://github.com/jemalloc/jemalloc.git
            GIT_TAG 5.3.0
        )
        FetchContent_GetProperties(jemalloc)
        if(NOT jemalloc_POPULATED)
            FetchContent_Populate(jemalloc)
            
            # jemalloc uses autotools, so we need to configure and build it
            set(JEMALLOC_SOURCE_DIR ${jemalloc_SOURCE_DIR})
            set(JEMALLOC_BINARY_DIR ${jemalloc_BINARY_DIR})
            set(JEMALLOC_INSTALL_DIR ${JEMALLOC_BINARY_DIR}/install)
            
            # Configure jemalloc with autoconf
            if(NOT EXISTS ${JEMALLOC_INSTALL_DIR}/lib/libjemalloc.a)
                message(STATUS "Configuring jemalloc...")
                execute_process(
                    COMMAND ./autogen.sh --prefix=${JEMALLOC_INSTALL_DIR} --disable-shared --enable-static --with-jemalloc-prefix=je_
                    WORKING_DIRECTORY ${JEMALLOC_SOURCE_DIR}
                    RESULT_VARIABLE JEMALLOC_AUTOGEN_RESULT
                    OUTPUT_QUIET
                    ERROR_QUIET
                )
                
                if(JEMALLOC_AUTOGEN_RESULT EQUAL 0)
                    message(STATUS "Building jemalloc...")
                    execute_process(
                        COMMAND make -j${CMAKE_BUILD_PARALLEL_LEVEL}
                        WORKING_DIRECTORY ${JEMALLOC_SOURCE_DIR}
                        RESULT_VARIABLE JEMALLOC_MAKE_RESULT
                        OUTPUT_QUIET
                        ERROR_QUIET
                    )
                    
                    if(JEMALLOC_MAKE_RESULT EQUAL 0)
                        execute_process(
                            COMMAND make install
                            WORKING_DIRECTORY ${JEMALLOC_SOURCE_DIR}
                            RESULT_VARIABLE JEMALLOC_INSTALL_RESULT
                            OUTPUT_QUIET
                            ERROR_QUIET
                        )
                    endif()
                endif()
            endif()
            
            if(EXISTS ${JEMALLOC_INSTALL_DIR}/lib/libjemalloc.a)
                message(STATUS "Successfully built jemalloc")
                set(HAVE_JEMALLOC TRUE)
                set(JEMALLOC_TARGET_NAME "jemalloc_fetched")
                add_library(${JEMALLOC_TARGET_NAME} INTERFACE)
                target_include_directories(${JEMALLOC_TARGET_NAME} INTERFACE ${JEMALLOC_INSTALL_DIR}/include)
                target_link_libraries(${JEMALLOC_TARGET_NAME} INTERFACE 
                    ${JEMALLOC_INSTALL_DIR}/lib/libjemalloc.a
                    pthread
                    dl
                )
            else()
                message(WARNING "Failed to build jemalloc - jemalloc benchmarks disabled")
                message(WARNING "You may need to install autoconf, automake, and libtool")
                set(HAVE_JEMALLOC FALSE)
            endif()
        endif()
    endif()
endif()

if(NEXUSALLOC_BENCH_TCMALLOC)
    # First try to find system tcmalloc
    find_library(TCMALLOC_LIBRARY
        NAMES tcmalloc_minimal tcmalloc
        HINTS /usr/lib /usr/local/lib /opt/gperftools/lib
    )
    find_path(TCMALLOC_INCLUDE_DIR
        NAMES gperftools/tcmalloc.h
        HINTS /usr/include /usr/local/include /opt/gperftools/include
    )

    if(TCMALLOC_LIBRARY AND TCMALLOC_INCLUDE_DIR)
        message(STATUS "Found system tcmalloc: ${TCMALLOC_LIBRARY}")
        set(HAVE_TCMALLOC TRUE)
        set(TCMALLOC_TARGET_NAME "tcmalloc_system")
        add_library(${TCMALLOC_TARGET_NAME} INTERFACE)
        target_include_directories(${TCMALLOC_TARGET_NAME} INTERFACE ${TCMALLOC_INCLUDE_DIR})
        target_link_libraries(${TCMALLOC_TARGET_NAME} INTERFACE ${TCMALLOC_LIBRARY})
    else()
        message(STATUS "System tcmalloc not found - fetching from source...")
        FetchContent_Declare(
            gperftools
            GIT_REPOSITORY https://github.com/gperftools/gperftools.git
            GIT_TAG gperftools-2.15
        )
        FetchContent_GetProperties(gperftools)
        if(NOT gperftools_POPULATED)
            FetchContent_Populate(gperftools)
            
            set(GPERFTOOLS_SOURCE_DIR ${gperftools_SOURCE_DIR})
            set(GPERFTOOLS_BINARY_DIR ${gperftools_BINARY_DIR})
            set(GPERFTOOLS_INSTALL_DIR ${GPERFTOOLS_BINARY_DIR}/install)
            
            # Configure gperftools with autoconf
            if(NOT EXISTS ${GPERFTOOLS_INSTALL_DIR}/lib/libtcmalloc_minimal.a)
                message(STATUS "Configuring gperftools...")
                execute_process(
                    COMMAND ./autogen.sh
                    WORKING_DIRECTORY ${GPERFTOOLS_SOURCE_DIR}
                    RESULT_VARIABLE GPERFTOOLS_AUTOGEN_RESULT
                    OUTPUT_QUIET
                    ERROR_QUIET
                )
                
                if(GPERFTOOLS_AUTOGEN_RESULT EQUAL 0)
                    execute_process(
                        COMMAND ./configure --prefix=${GPERFTOOLS_INSTALL_DIR} --disable-shared --enable-static --enable-minimal
                        WORKING_DIRECTORY ${GPERFTOOLS_SOURCE_DIR}
                        RESULT_VARIABLE GPERFTOOLS_CONFIGURE_RESULT
                        OUTPUT_QUIET
                        ERROR_QUIET
                    )
                    
                    if(GPERFTOOLS_CONFIGURE_RESULT EQUAL 0)
                        message(STATUS "Building gperftools...")
                        execute_process(
                            COMMAND make -j${CMAKE_BUILD_PARALLEL_LEVEL}
                            WORKING_DIRECTORY ${GPERFTOOLS_SOURCE_DIR}
                            RESULT_VARIABLE GPERFTOOLS_MAKE_RESULT
                            OUTPUT_QUIET
                            ERROR_QUIET
                        )
                        
                        if(GPERFTOOLS_MAKE_RESULT EQUAL 0)
                            execute_process(
                                COMMAND make install
                                WORKING_DIRECTORY ${GPERFTOOLS_SOURCE_DIR}
                                RESULT_VARIABLE GPERFTOOLS_INSTALL_RESULT
                                OUTPUT_QUIET
                                ERROR_QUIET
                            )
                        endif()
                    endif()
                endif()
            endif()
            
            if(EXISTS ${GPERFTOOLS_INSTALL_DIR}/lib/libtcmalloc_minimal.a)
                message(STATUS "Successfully built tcmalloc")
                set(HAVE_TCMALLOC TRUE)
                set(TCMALLOC_TARGET_NAME "tcmalloc_fetched")
                add_library(${TCMALLOC_TARGET_NAME} INTERFACE)
                target_include_directories(${TCMALLOC_TARGET_NAME} INTERFACE ${GPERFTOOLS_INSTALL_DIR}/include)
                target_link_libraries(${TCMALLOC_TARGET_NAME} INTERFACE 
                    ${GPERFTOOLS_INSTALL_DIR}/lib/libtcmalloc_minimal.a
                    pthread
                )
            else()
                message(WARNING "Failed to build tcmalloc - tcmalloc benchmarks disabled")
                message(WARNING "You may need to install autoconf, automake, and libtool")
                set(HAVE_TCMALLOC FALSE)
            endif()
        endif()
    endif()
endif()

# ==============================================================================
# Basic allocator benchmark (NexusAlloc vs malloc)
# ==============================================================================
add_executable(bench_allocator
    bench_allocator.cpp
)

target_link_libraries(bench_allocator PRIVATE
    nexusalloc
    benchmark::benchmark
    pthread
)

add_executable(bench_comparison
    bench_comparison.cpp
)

target_link_libraries(bench_comparison PRIVATE
    nexusalloc
    benchmark::benchmark
    pthread
)

# Link jemalloc if available
if(HAVE_JEMALLOC)
    target_link_libraries(bench_comparison PRIVATE ${JEMALLOC_TARGET_NAME})
    target_compile_definitions(bench_comparison PRIVATE NEXUSALLOC_HAS_JEMALLOC=1)
endif()

# Link tcmalloc if available
if(HAVE_TCMALLOC)
    target_link_libraries(bench_comparison PRIVATE ${TCMALLOC_TARGET_NAME})
    target_compile_definitions(bench_comparison PRIVATE NEXUSALLOC_HAS_TCMALLOC=1)
endif()

message(STATUS "")
message(STATUS "Benchmark configuration summary:")
message(STATUS "  - Basic benchmark (bench_allocator):      ON")
message(STATUS "  - Comparison benchmark (bench_comparison): ON")
message(STATUS "    - jemalloc support:  ${HAVE_JEMALLOC}")
message(STATUS "    - tcmalloc support:  ${HAVE_TCMALLOC}")
if(NOT HAVE_JEMALLOC OR NOT HAVE_TCMALLOC)
    message(STATUS "")
    message(STATUS "  To enable all allocator benchmarks, install build dependencies:")
    message(STATUS "    Ubuntu/Debian: sudo apt install autoconf automake libtool")
    message(STATUS "    Fedora/RHEL:   sudo dnf install autoconf automake libtool")
    message(STATUS "    Or install the allocators directly:")
    message(STATUS "    Ubuntu/Debian: sudo apt install libjemalloc-dev libgoogle-perftools-dev")
endif()
message(STATUS "")
