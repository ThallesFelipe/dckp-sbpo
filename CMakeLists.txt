cmake_minimum_required(VERSION 3.24)

project(dckp_sbpo LANGUAGES CXX)

include(CheckCXXCompilerFlag)
include(CheckCXXSourceCompiles)
include(CTest)

option(DCKP_ENABLE_SANITIZERS "Enable AddressSanitizer/UBSan in Debug builds" ON)
option(DCKP_ENABLE_HARDENING "Enable hardening flags" ON)
option(DCKP_BUILD_TESTS "Build unit/regression tests" ON)

# The CMake-level standard the project asks for. C++23 is the portable default
# (GCC 13+, Clang 16+, MSVC 19.35+). Override with -DCMAKE_CXX_STANDARD=26 when
# an experimental C++26 toolchain is available (Makefile's `debug26` target).
if(NOT DEFINED CMAKE_CXX_STANDARD)
    set(CMAKE_CXX_STANDARD 23)
endif()
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Probe whether the active toolchain can actually link ASan+UBSan. MinGW
# distributions frequently ship the compiler without the sanitizer runtimes,
# so the flag is accepted at compile time but fails at link time. We test
# the full compile+link round trip before enabling.
set(DCKP_SANITIZERS_AVAILABLE OFF)
if(NOT MSVC AND DCKP_ENABLE_SANITIZERS)
    set(CMAKE_REQUIRED_FLAGS "-fsanitize=address,undefined")
    set(CMAKE_REQUIRED_LINK_OPTIONS "-fsanitize=address,undefined")
    check_cxx_source_compiles("int main(){return 0;}" DCKP_SANITIZERS_AVAILABLE)
    unset(CMAKE_REQUIRED_FLAGS)
    unset(CMAKE_REQUIRED_LINK_OPTIONS)
    if(NOT DCKP_SANITIZERS_AVAILABLE)
        message(STATUS "Sanitizers requested but not linkable on this toolchain; disabling.")
    endif()
endif()

# ----------------------------------------------------------------------------
# Core static library: shared between the CLI executable and the tests.
# ----------------------------------------------------------------------------
add_library(dckp_core STATIC
    src/utils/instance_reader.cpp
    src/utils/solution.cpp
    src/utils/validator.cpp
    src/algorithms/runner.cpp
    src/constructive/greedy_max_profit.cpp
)
target_include_directories(dckp_core PUBLIC src)

# ----------------------------------------------------------------------------
# CLI executable.
# ----------------------------------------------------------------------------
add_executable(dckp_sbpo src/main.cpp)
target_link_libraries(dckp_sbpo PRIVATE dckp_core)

# ----------------------------------------------------------------------------
# Compiler options applied to all project targets.
# ----------------------------------------------------------------------------
function(dckp_apply_project_flags tgt)
    if(MINGW)
        # Ship GCC+STL statically so binaries don't depend on libgcc_s_*.dll /
        # libstdc++-6.dll being discoverable at runtime (ctest's sanitized
        # PATH typically cannot find them).
        target_link_options(${tgt} PRIVATE -static-libgcc -static-libstdc++)
    endif()
    if(MSVC)
        target_compile_options(${tgt} PRIVATE /W4 /permissive- /EHsc /utf-8)
        if(DCKP_ENABLE_HARDENING)
            target_compile_options(${tgt} PRIVATE /sdl /GS)
            target_compile_definitions(${tgt} PRIVATE $<$<CONFIG:Debug>:_ITERATOR_DEBUG_LEVEL=2>)
        endif()
    else()
        target_compile_options(${tgt} PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Wconversion
            -Wshadow
            -Wformat=2
            -Wundef
            $<$<CONFIG:Debug>:-O0>
            $<$<CONFIG:Debug>:-g3>
            $<$<CONFIG:Debug>:-fno-omit-frame-pointer>
        )
        if(DCKP_ENABLE_HARDENING)
            target_compile_options(${tgt} PRIVATE -fstack-protector-strong)
            # _FORTIFY_SOURCE requires at least -O1, otherwise GCC warns and it
            # is a no-op — only enable outside of -O0 Debug builds.
            target_compile_definitions(${tgt} PRIVATE
                $<$<NOT:$<CONFIG:Debug>>:_FORTIFY_SOURCE=3>
                $<$<CONFIG:Debug>:_GLIBCXX_ASSERTIONS=1>
            )
            if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
                target_link_options(${tgt} PRIVATE -Wl,-z,relro,-z,now)
            endif()
        endif()
        if(DCKP_ENABLE_SANITIZERS AND DCKP_SANITIZERS_AVAILABLE)
            target_compile_options(${tgt} PRIVATE
                $<$<CONFIG:Debug>:-fsanitize=address,undefined>
                $<$<CONFIG:Debug>:-fno-sanitize-recover=all>
            )
            target_link_options(${tgt} PRIVATE
                $<$<CONFIG:Debug>:-fsanitize=address,undefined>
            )
        endif()
    endif()
endfunction()

dckp_apply_project_flags(dckp_core)
dckp_apply_project_flags(dckp_sbpo)

# ----------------------------------------------------------------------------
# Tests.
# ----------------------------------------------------------------------------
if(DCKP_BUILD_TESTS AND BUILD_TESTING)
    enable_testing()
    add_subdirectory(tests)
endif()
