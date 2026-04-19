cmake_minimum_required(VERSION 3.24)

project(dckp_sbpo LANGUAGES CXX)

include(CheckCXXSourceCompiles)
include(CTest)

if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
    message(FATAL_ERROR "This project supports only Linux builds (target workflow: WSL Ubuntu 24.04).")
endif()

option(DCKP_ENABLE_SANITIZERS "Enable AddressSanitizer/UBSan" OFF)
option(DCKP_ENABLE_HARDENING "Enable hardening flags" ON)
option(DCKP_BUILD_TESTS "Build unit/regression tests" ON)

if(NOT DEFINED CMAKE_CXX_STANDARD)
    set(CMAKE_CXX_STANDARD 23)
endif()
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(DCKP_SANITIZERS_AVAILABLE OFF)
if(DCKP_ENABLE_SANITIZERS)
    set(CMAKE_REQUIRED_FLAGS "-fsanitize=address,undefined")
    set(CMAKE_REQUIRED_LINK_OPTIONS "-fsanitize=address,undefined")
    check_cxx_source_compiles("int main(){return 0;}" DCKP_SANITIZERS_AVAILABLE)
    unset(CMAKE_REQUIRED_FLAGS)
    unset(CMAKE_REQUIRED_LINK_OPTIONS)
    if(NOT DCKP_SANITIZERS_AVAILABLE)
        message(STATUS "Sanitizers requested but not linkable on this toolchain; disabling.")
    endif()
endif()

add_library(dckp_core STATIC
    src/utils/instance_reader.cpp
    src/utils/solution.cpp
    src/utils/validator.cpp
    src/algorithms/runner.cpp
    src/constructive/greedy_max_profit.cpp
    src/local_search/vnd.cpp
    src/local_search/ils.cpp
    src/local_search/vns.cpp
)
target_include_directories(dckp_core PUBLIC src)

add_executable(dckp_sbpo src/main.cpp)
target_link_libraries(dckp_sbpo PRIVATE dckp_core)

function(dckp_apply_project_flags tgt)
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
        target_compile_definitions(${tgt} PRIVATE
            $<$<NOT:$<CONFIG:Debug>>:_FORTIFY_SOURCE=3>
            $<$<CONFIG:Debug>:_GLIBCXX_ASSERTIONS=1>
        )
        target_link_options(${tgt} PRIVATE -Wl,-z,relro,-z,now)
    endif()
    if(DCKP_ENABLE_SANITIZERS AND DCKP_SANITIZERS_AVAILABLE)
        target_compile_options(${tgt} PRIVATE
            -fsanitize=address,undefined
            -fno-sanitize-recover=all
        )
        target_link_options(${tgt} PRIVATE -fsanitize=address,undefined)
    endif()
endfunction()

dckp_apply_project_flags(dckp_core)
dckp_apply_project_flags(dckp_sbpo)

if(DCKP_BUILD_TESTS AND BUILD_TESTING)
    enable_testing()
    add_subdirectory(tests)
endif()
