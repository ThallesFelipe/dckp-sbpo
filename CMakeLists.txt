cmake_minimum_required(VERSION 3.28)

project(dckp_sbpo LANGUAGES CXX)

include(CheckCXXCompilerFlag)

option(DCKP_ENABLE_SANITIZERS "Enable AddressSanitizer/UBSan in Debug builds" ON)
option(DCKP_ENABLE_HARDENING "Enable hardening flags" ON)

add_executable(
    dckp_sbpo
    src/main.cpp
    src/utils/instance_reader.cpp
    src/utils/solution.cpp
    src/utils/validator.cpp
)

if(MSVC)
    target_compile_options(dckp_sbpo PRIVATE /W4 /permissive- /EHsc /utf-8 /std:c++latest)
else()
    check_cxx_compiler_flag("-std=c++2c" DCKP_HAS_STD_CXX2C)

    if(NOT DCKP_HAS_STD_CXX2C)
        message(FATAL_ERROR "Compiler sem suporte a C++26 (-std=c++2c). Use GCC/G++ 14+ no Ubuntu 24.04 WSL.")
    endif()

    target_compile_options(
        dckp_sbpo
        PRIVATE
        -std=c++2c
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
        target_compile_options(dckp_sbpo PRIVATE -fstack-protector-strong -D_FORTIFY_SOURCE=3)
        target_link_options(dckp_sbpo PRIVATE -Wl,-z,relro,-z,now)
        target_compile_definitions(dckp_sbpo PRIVATE $<$<CONFIG:Debug>:_GLIBCXX_ASSERTIONS=1>)
    endif()

    if(DCKP_ENABLE_SANITIZERS)
        target_compile_options(
            dckp_sbpo
            PRIVATE
            $<$<CONFIG:Debug>:-fsanitize=address,undefined>
            $<$<CONFIG:Debug>:-fno-sanitize-recover=all>
        )
        target_link_options(dckp_sbpo PRIVATE $<$<CONFIG:Debug>:-fsanitize=address,undefined>)
    endif()
endif()