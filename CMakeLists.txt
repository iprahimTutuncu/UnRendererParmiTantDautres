cmake_minimum_required(VERSION 3.21 FATAL_ERROR)

project(Olaf
    VERSION 0.1
    LANGUAGES CXX
)

#  ========== CMAKE CONFIGURATION ==========

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS YES)

option(OlafEnableLTO "Enable LTO" OFF)

# Set build type
if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

if (WIN32 OR MSVC)
    add_compile_options(/utf-8)
endif()

# Optimization
if (NOT (WIN32 OR MSVC))
    add_compile_options(-march=native -mtune=native)
endif()

# IPO / LTO
include(CheckIPOSupported)
check_ipo_supported(RESULT ipo_supported OUTPUT output)
if(NOT ipo_supported)
    message(WARNING "IPO / LTO is not supported: ${output}")
else()
    if (OlafEnableLTO)
        message(STATUS "IPO / LTO enabled")
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
    else()
        message(STATUS "IPO / LTO disabled")
    endif()
endif()

#  ========== LIBRARIES ==========

# OpenGL - The Industry Standard for High Performance Graphics
find_package(OpenGL 4.5 REQUIRED)

# Simple DirectMedia Layer (SDL for short)
set(SDL_STATIC ON)
set(SDL_SHARED OFF)
add_subdirectory(libs/SDL EXCLUDE_FROM_ALL)

# ImGui: bloat-free graphical user interface library for C++
add_library(ImGui STATIC
    libs/imgui/imgui.cpp
    libs/imgui/imgui_draw.cpp
    libs/imgui/imgui_tables.cpp
    libs/imgui/imgui_widgets.cpp
    # Here implementation
    libs/imgui/backends/imgui_impl_sdl3.cpp
    libs/imgui/backends/imgui_impl_sdlgpu3.cpp
)
target_include_directories(ImGui PUBLIC libs/imgui)
target_include_directories(ImGui PRIVATE libs/imgui/backends)
target_link_libraries(ImGui PRIVATE
    SDL3::SDL3
)

# OpenGL Mathematics (GLM)
add_subdirectory(libs/glm)

# spdlog: Fast C++ logging library
set(SPDLOG_ENABLE_PCH ON)
set(SPDLOG_BUILD_PIC ON)
add_subdirectory(libs/spdlog)

#  ========== PROJECT ==========

# Add warning flags for Windows and Linux
if (WIN32 OR MSVC)
    add_compile_options(/EHsc /W4 /WX /wd4458)
else()
    add_compile_options(-Wall -Wextra -pedantic -Werror)
endif()

if (NOT (CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "MinSizeRel"))
    add_compile_definitions(DEBUG=1)
endif()

# go to the different modules
add_subdirectory(Engine)
add_subdirectory(application)

#  ========== TESTS ==========
