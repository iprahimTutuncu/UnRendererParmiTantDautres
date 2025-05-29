cmake_minimum_required(VERSION 3.21 FATAL_ERROR)

project(Olaf
    VERSION 0.1
    LANGUAGES CXX
)

#  ========== CMAKE CONFIGURATION ==========

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS YES)

# Set build type
if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "" FORCE)
endif()

# Optimization
if (WIN32 OR MSVC)
    add_compile_options(/utf-8)
else()
    add_compile_options(-march=native -mtune=native)
endif()

# IPO / LTO
include(CheckIPOSupported)
check_ipo_supported(RESULT ipo_supported OUTPUT output)
if(NOT ipo_supported)
    message(WARNING "IPO / LTO is not supported: ${output}")
else()
    message(STATUS "IPO / LTO enabled")
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
endif()

#  ========== LIBRARIES ==========

# OpenGL - The Industry Standard for High Performance Graphics
find_package(OpenGL 4.5 REQUIRED)

# Simple DirectMedia Layer (SDL for short)
set(SDL_STATIC ON)
set(SDL_SHARED OFF)
add_subdirectory(thirdParty/SDL EXCLUDE_FROM_ALL)

# ImGui: bloat-free graphical user interface library for C++
add_library(ImGui STATIC
    thirdParty/imgui/imgui.cpp
    thirdParty/imgui/imgui_draw.cpp
    thirdParty/imgui/imgui_tables.cpp
    thirdParty/imgui/imgui_widgets.cpp
    # Here implementation
    thirdParty/imgui/backends/imgui_impl_sdl3.cpp
    thirdParty/imgui/backends/imgui_impl_sdlgpu3.cpp
)
target_include_directories(ImGui PUBLIC thirdParty/imgui)
target_include_directories(ImGui PRIVATE thirdParty/imgui/backends)
target_link_libraries(ImGui PRIVATE
    SDL3::SDL3
)

# OpenGL Mathematics (GLM)
add_subdirectory(thirdParty/glm)

# spdlog: Fast C++ logging library
set(SPDLOG_ENABLE_PCH ON)
set(SPDLOG_BUILD_PIC ON)
add_subdirectory(thirdParty/spdlog)

#  ========== PROJECT ==========

# Add warning flags for Windows and Linux
if (WIN32 OR MSVC)
    add_compile_options(/EHsc /W4)
else()
    add_compile_options(-Wall -Wextra -pedantic)
endif()


# go to the different modules
add_subdirectory(Engine)
add_subdirectory(application)


#  ========== TESTS ==========
