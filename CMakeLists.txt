# Morphect - Multi-Language Code Obfuscator
# CMake Build System

cmake_minimum_required(VERSION 3.20)

project(morphect
    VERSION 1.0.1
    DESCRIPTION "Multi-Language Code Obfuscator"
    HOMEPAGE_URL "https://github.com/morphect/morphect"
    LANGUAGES CXX C
)

# C++17 required
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# ============================================================================
# Options
# ============================================================================
option(MORPHECT_BUILD_TESTS "Build tests" OFF)
option(MORPHECT_BUILD_BENCHMARKS "Build benchmarks" OFF)
option(MORPHECT_BUILD_GIMPLE_PLUGIN "Build GCC GIMPLE plugin" ON)
option(MORPHECT_BUILD_IR_OBFUSCATOR "Build LLVM IR obfuscator" ON)
option(MORPHECT_BUILD_ASM_OBFUSCATOR "Build assembly obfuscator" ON)
option(MORPHECT_USE_FETCHCONTENT "Use FetchContent for dependencies" ON)
option(MORPHECT_INSTALL "Enable install targets" ON)
option(MORPHECT_COVERAGE "Enable code coverage" OFF)

# Compiler flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic")
# Note: Sanitizers are NOT enabled by default in Debug builds.
# Use the CI sanitizers job or pass -DCMAKE_CXX_FLAGS="-fsanitize=address" explicitly.
# This prevents issues with ASan runtime preloading during test discovery.
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -DDEBUG")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O2 -g -DNDEBUG")

# Coverage flags
if(MORPHECT_COVERAGE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --coverage -fprofile-arcs -ftest-coverage")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} --coverage -fprofile-arcs -ftest-coverage")
endif()

# ============================================================================
# Dependencies via FetchContent
# ============================================================================
include(FetchContent)

if(MORPHECT_BUILD_TESTS AND MORPHECT_USE_FETCHCONTENT)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.14.0
        GIT_SHALLOW TRUE
    )
    # Prevent GoogleTest from overriding compiler flags
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
endif()

if(MORPHECT_BUILD_BENCHMARKS AND MORPHECT_USE_FETCHCONTENT)
    FetchContent_Declare(
        benchmark
        GIT_REPOSITORY https://github.com/google/benchmark.git
        GIT_TAG v1.8.3
        GIT_SHALLOW TRUE
    )
    set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
    set(BENCHMARK_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(benchmark)
endif()

# ============================================================================
# Include directories
# ============================================================================
include_directories(${CMAKE_SOURCE_DIR}/src)

# ============================================================================
# Core Library (header-only)
# ============================================================================
add_library(morphect_core INTERFACE)
target_include_directories(morphect_core INTERFACE
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
    $<INSTALL_INTERFACE:include/morphect>
)

# ============================================================================
# MBA Library (for LLVM IR obfuscator)
# ============================================================================
add_library(morphect_mba STATIC
    src/passes/mba/mba_add.cpp
    src/passes/mba/mba_sub.cpp
    src/passes/mba/mba_xor.cpp
    src/passes/mba/mba_and.cpp
    src/passes/mba/mba_or.cpp
    src/passes/mba/mba_mult.cpp
)
target_link_libraries(morphect_mba PUBLIC morphect_core)

# ============================================================================
# Data Obfuscation Library
# ============================================================================
add_library(morphect_data STATIC
    src/passes/data/string_encoding.cpp
    src/passes/data/constant_obf.cpp
    src/passes/data/variable_splitting.cpp
)
target_link_libraries(morphect_data PUBLIC morphect_core)

# ============================================================================
# Control Flow Obfuscation Library
# ============================================================================
add_library(morphect_cff STATIC
    src/passes/cff/llvm_cfg_analyzer.cpp
    src/passes/cff/llvm_cff_transform.cpp
)
target_link_libraries(morphect_cff PUBLIC morphect_core)

# ============================================================================
# Control Flow Library (Indirect Branches, Calls, and Call Stack Obfuscation)
# ============================================================================
add_library(morphect_control_flow STATIC
    src/passes/control_flow/indirect_branch.cpp
    src/passes/control_flow/indirect_call.cpp
    src/passes/control_flow/call_stack_obf.cpp
)
target_link_libraries(morphect_control_flow PUBLIC morphect_core morphect_mba)

# ============================================================================
# Dead Code Library (Smart dead code insertion)
# ============================================================================
add_library(morphect_deadcode STATIC
    src/passes/deadcode/dead_code.cpp
)
target_link_libraries(morphect_deadcode PUBLIC morphect_core)

# ============================================================================
# Anti-Disassembly Library
# ============================================================================
add_library(morphect_antidisasm STATIC
    src/passes/antidisasm/antidisasm.cpp
)
target_link_libraries(morphect_antidisasm PUBLIC morphect_core)

# ============================================================================
# Anti-Debugging Library
# ============================================================================
add_library(morphect_antidebug STATIC
    src/passes/antidebug/antidebug.cpp
)
target_link_libraries(morphect_antidebug PUBLIC morphect_core)

# ============================================================================
# GCC GIMPLE Plugin
# ============================================================================
if(MORPHECT_BUILD_GIMPLE_PLUGIN)
    # Find GCC plugin directory using the actual C compiler (not just 'gcc')
    # This ensures we use the correct plugin headers when CC=gcc-12/gcc-13/etc
    execute_process(
        COMMAND ${CMAKE_C_COMPILER} -print-file-name=plugin
        OUTPUT_VARIABLE GCC_PLUGIN_DIR
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    if(EXISTS "${GCC_PLUGIN_DIR}/include")
        message(STATUS "Found GCC plugin directory: ${GCC_PLUGIN_DIR}")

        add_library(morphect_plugin SHARED
            src/backends/gimple/gimple_obf_plugin.cpp
        )

        target_include_directories(morphect_plugin PRIVATE
            ${GCC_PLUGIN_DIR}/include
            ${CMAKE_SOURCE_DIR}/src
        )

        target_compile_options(morphect_plugin PRIVATE
            -fPIC
            -fno-rtti
            -Wno-literal-suffix
        )

        set_target_properties(morphect_plugin PROPERTIES
            OUTPUT_NAME "morphect_plugin"
            PREFIX ""
            SUFFIX ".so"
        )

        if(MORPHECT_INSTALL)
            install(TARGETS morphect_plugin
                LIBRARY DESTINATION lib/morphect
                COMPONENT plugin
            )
        endif()
    else()
        message(WARNING "GCC plugin support not found, skipping GIMPLE plugin")
        set(MORPHECT_BUILD_GIMPLE_PLUGIN OFF)
    endif()
endif()

# ============================================================================
# LLVM IR Obfuscator
# ============================================================================
if(MORPHECT_BUILD_IR_OBFUSCATOR)
    add_executable(morphect-ir
        src/backends/llvm_ir/ir_obfuscator.cpp
    )

    target_link_libraries(morphect-ir PRIVATE
        morphect_core
        morphect_mba
        morphect_data
        morphect_cff
        morphect_deadcode
        morphect_control_flow
    )

    if(MORPHECT_INSTALL)
        install(TARGETS morphect-ir
            RUNTIME DESTINATION bin
            COMPONENT tools
        )
    endif()
endif()

# ============================================================================
# Assembly Obfuscator
# ============================================================================
if(MORPHECT_BUILD_ASM_OBFUSCATOR)
    add_executable(morphect-asm
        src/backends/assembly/assembly_obfuscator.cpp
    )

    target_link_libraries(morphect-asm PRIVATE
        morphect_core
    )

    if(MORPHECT_INSTALL)
        install(TARGETS morphect-asm
            RUNTIME DESTINATION bin
            COMPONENT tools
        )
    endif()
endif()

# ============================================================================
# Runtime Library (string decoder)
# ============================================================================
add_library(morphect_runtime STATIC
    src/runtime/string_decoder.c
)

set_target_properties(morphect_runtime PROPERTIES
    OUTPUT_NAME "morphect_runtime"
    C_STANDARD 11
)

if(MORPHECT_INSTALL)
    install(TARGETS morphect_runtime
        ARCHIVE DESTINATION lib
        COMPONENT runtime
    )
endif()

# ============================================================================
# Tests
# ============================================================================
if(MORPHECT_BUILD_TESTS)
    enable_testing()

    # Check if GoogleTest is available
    if(TARGET gtest)
        message(STATUS "Using GoogleTest from FetchContent")

        # Unit tests with GoogleTest
        add_executable(morphect_test
            tests/unit/test_main.cpp
            tests/unit/test_random.cpp
            tests/unit/test_statistics.cpp
            tests/unit/test_json.cpp
            tests/unit/test_mba.cpp
            tests/unit/test_cff.cpp
            tests/unit/test_data.cpp
            tests/unit/test_control_flow.cpp
            tests/unit/test_deadcode.cpp
            tests/unit/test_antidisasm.cpp
            tests/unit/test_antidebug.cpp
            tests/unit/test_coverage.cpp
            tests/unit/test_assembly.cpp
        )

        target_link_libraries(morphect_test PRIVATE
            morphect_core
            morphect_mba
            morphect_cff
            morphect_data
            morphect_control_flow
            morphect_deadcode
            morphect_antidisasm
            morphect_antidebug
            gtest
            gtest_main
        )

        include(GoogleTest)
        # Use PRE_TEST discovery mode to avoid running the test executable at configure time
        # This prevents issues with sanitizers (ASan/UBSan) that require proper preloading
        gtest_discover_tests(morphect_test
            PROPERTIES LABELS "unit"
            DISCOVERY_MODE PRE_TEST
        )

        # Integration tests (if source files exist)
        if(EXISTS "${CMAKE_SOURCE_DIR}/tests/integration/test_gimple.cpp")
            add_executable(morphect_integration_test
                tests/unit/test_main.cpp
                tests/integration/test_gimple.cpp
                tests/integration/test_ir.cpp
            )

            target_include_directories(morphect_integration_test PRIVATE
                ${CMAKE_SOURCE_DIR}/tests
            )

            target_link_libraries(morphect_integration_test PRIVATE
                morphect_core
                morphect_mba
                gtest
                gtest_main
            )

            gtest_discover_tests(morphect_integration_test
                PROPERTIES LABELS "integration"
                DISCOVERY_MODE PRE_TEST
            )
        endif()

        # Regression tests (if source files exist)
        if(EXISTS "${CMAKE_SOURCE_DIR}/tests/regression/test_regressions.cpp")
            add_executable(morphect_regression_test
                tests/unit/test_main.cpp
                tests/regression/test_regressions.cpp
            )

            target_include_directories(morphect_regression_test PRIVATE
                ${CMAKE_SOURCE_DIR}/tests
            )

            target_link_libraries(morphect_regression_test PRIVATE
                morphect_core
                morphect_mba
                gtest
                gtest_main
            )

            gtest_discover_tests(morphect_regression_test
                PROPERTIES LABELS "regression"
                DISCOVERY_MODE PRE_TEST
            )
        endif()

    else()
        message(STATUS "GoogleTest not available, using simple tests")

        # Simple test without GoogleTest
        add_executable(morphect_simple_test
            tests/unit/simple_test.cpp
        )

        target_link_libraries(morphect_simple_test PRIVATE
            morphect_core
        )

        add_test(NAME simple_test COMMAND morphect_simple_test)
    endif()

    # Integration test target
    add_custom_target(test_integration
        COMMAND ${CMAKE_SOURCE_DIR}/scripts/run_integration_tests.sh
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Running integration tests"
        DEPENDS morphect_plugin morphect-ir
    )
endif()

# ============================================================================
# Benchmarks
# ============================================================================
if(MORPHECT_BUILD_BENCHMARKS AND TARGET benchmark)
    add_executable(morphect_bench
        tests/benchmarks/bench_mba.cpp
    )

    target_link_libraries(morphect_bench PRIVATE
        morphect_core
        morphect_mba
        morphect_cff
        benchmark::benchmark
        benchmark::benchmark_main
    )

    add_custom_target(run_benchmarks
        COMMAND morphect_bench
        DEPENDS morphect_bench
        COMMENT "Running benchmarks"
    )
endif()

# ============================================================================
# Installation
# ============================================================================
if(MORPHECT_INSTALL)
    include(GNUInstallDirs)

    # Headers
    install(DIRECTORY src/
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/morphect
        COMPONENT headers
        FILES_MATCHING
        PATTERN "*.hpp"
        PATTERN "*.h"
        PATTERN "backends" EXCLUDE
    )

    # Config files
    install(DIRECTORY config/
        DESTINATION ${CMAKE_INSTALL_SYSCONFDIR}/morphect
        COMPONENT config
        FILES_MATCHING
        PATTERN "*.json"
    )

    # Runtime header for users
    install(FILES src/runtime/string_decoder.c
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/morphect/runtime
        COMPONENT runtime
    )

    # CMake package config
    include(CMakePackageConfigHelpers)

    configure_package_config_file(
        ${CMAKE_SOURCE_DIR}/cmake/MorphectConfig.cmake.in
        ${CMAKE_BINARY_DIR}/MorphectConfig.cmake
        INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/morphect
    )

    write_basic_package_version_file(
        ${CMAKE_BINARY_DIR}/MorphectConfigVersion.cmake
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY SameMajorVersion
    )

    install(FILES
        ${CMAKE_BINARY_DIR}/MorphectConfig.cmake
        ${CMAKE_BINARY_DIR}/MorphectConfigVersion.cmake
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/morphect
        COMPONENT cmake
    )

    # Export targets
    install(TARGETS morphect_core morphect_mba morphect_data morphect_runtime
        EXPORT MorphectTargets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )

    install(EXPORT MorphectTargets
        FILE MorphectTargets.cmake
        NAMESPACE Morphect::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/morphect
    )

    # CPack configuration
    set(CPACK_PACKAGE_NAME "morphect")
    set(CPACK_PACKAGE_VENDOR "Morphect")
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY ${PROJECT_DESCRIPTION})
    set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
    set(CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
    set(CPACK_PACKAGE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
    set(CPACK_PACKAGE_VERSION_PATCH ${PROJECT_VERSION_PATCH})

    set(CPACK_GENERATOR "TGZ;DEB;RPM")
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Morphect Team")
    set(CPACK_DEBIAN_PACKAGE_DEPENDS "gcc (>= 12)")

    include(CPack)
endif()

# ============================================================================
# Summary
# ============================================================================
message(STATUS "")
message(STATUS "======================================")
message(STATUS "Morphect v${PROJECT_VERSION} Configuration")
message(STATUS "======================================")
message(STATUS "Build type:        ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ Standard:      ${CMAKE_CXX_STANDARD}")
message(STATUS "Install prefix:    ${CMAKE_INSTALL_PREFIX}")
message(STATUS "")
message(STATUS "Components:")
message(STATUS "  GIMPLE Plugin:   ${MORPHECT_BUILD_GIMPLE_PLUGIN}")
message(STATUS "  IR Obfuscator:   ${MORPHECT_BUILD_IR_OBFUSCATOR}")
message(STATUS "  ASM Obfuscator:  ${MORPHECT_BUILD_ASM_OBFUSCATOR}")
message(STATUS "  Tests:           ${MORPHECT_BUILD_TESTS}")
message(STATUS "  Benchmarks:      ${MORPHECT_BUILD_BENCHMARKS}")
message(STATUS "  Install:         ${MORPHECT_INSTALL}")
message(STATUS "======================================")
message(STATUS "")
