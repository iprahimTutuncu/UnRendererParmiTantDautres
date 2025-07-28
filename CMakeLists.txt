cmake_minimum_required(VERSION 3.21 FATAL_ERROR)

project(Olaf
    VERSION 0.2.0
    LANGUAGES CXX CUDA
)

#  ========== CMAKE CONFIGURATION ==========

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS YES)
set(CMAKE_CUDA_STANDARD 20)
set(CMAKE_CUDA_ARCHITECTURES "native")

option(OlafEnableLTO "Enable LTO" OFF)

# Set build type
if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

if (WIN32 OR MSVC)
    add_compile_options(/utf-8 /arch:AVX2)
endif()

# Optimization
if (NOT (WIN32 OR MSVC))
    add_compile_options(-march=native -mtune=native -mavx2)
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

find_package(OpenMP)


# Simple DirectMedia Layer (SDL for short)
set(SDL_STATIC ON)
set(SDL_SHARED OFF)
add_subdirectory(libs/SDL EXCLUDE_FROM_ALL)

# ImGui: bloat-free graphical user interface library for C++
add_library(ImGui STATIC EXCLUDE_FROM_ALL
    libs/imgui/imgui.cpp
    libs/imgui/imgui_draw.cpp
    libs/imgui/imgui_tables.cpp
    libs/imgui/imgui_widgets.cpp
    # Here implementation
    libs/imgui/backends/imgui_impl_sdl3.cpp
    libs/imgui/backends/imgui_impl_sdlgpu3.cpp
)
target_include_directories(ImGui PUBLIC libs/imgui libs/imgui/backends)
target_link_libraries(ImGui PRIVATE
    SDL3::SDL3
)

# stb_image: C library for loading images into memory with minimal dependencies
set(GENERATED_STB_WRAPPER ${CMAKE_CURRENT_BINARY_DIR}/stb_wrapper.cpp)
add_custom_command(
    OUTPUT ${GENERATED_STB_WRAPPER}
    COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_SOURCE_DIR}/libs/stb/stb_image.h
        ${GENERATED_STB_WRAPPER}
    DEPENDS libs/stb/stb_image.h
    COMMENT "Generating stb_wrapper.cpp"
)

# OpenGL Mathematics (GLM)
add_subdirectory(libs/glm EXCLUDE_FROM_ALL)
target_compile_definitions(glm INTERFACE GLM_ENABLE_EXPERIMENTAL)

# Eigen: C++ template library for linear algebra
set(EIGEN_BUILD_TESTING OFF)
set(EIGEN_INCLUDE_INSTALL_DIR OFF)
add_subdirectory(libs/eigen EXCLUDE_FROM_ALL)
target_compile_definitions(eigen INTERFACE EIGEN_NO_DEBUG)

# UTL: C++ utilities library (profiling)
add_subdirectory(libs/UTL EXCLUDE_FROM_ALL)
target_compile_definitions(utl_lib_include INTERFACE
    UTL_PROFILER_USE_INTRINSICS_FOR_FREQUENCY=2995.2e6
)

#  ========== SHADERS ==========

set(SHADER_DIR ${CMAKE_CURRENT_SOURCE_DIR}/shaders)
set(SHADER_OUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/shaders)
set(GLSLANG_EXE ${CMAKE_CURRENT_SOURCE_DIR}/tools/glslang)
set(SPIRV_OPT_EXE ${CMAKE_CURRENT_SOURCE_DIR}/tools/spirv-opt)
if (WIN32 OR MSVC)
    set(GLSLANG_EXE ${GLSLANG_EXE}.exe)
    set(SPIRV_OPT_EXE ${SPIRV_OPT_EXE}.exe)
endif()

set(SHADERS
    ${SHADER_DIR}/deferred_debug.frag
    ${SHADER_DIR}/deferred_gBuffer.frag
    ${SHADER_DIR}/deferred_gBuffer_particles.vert
    ${SHADER_DIR}/deferred_gBuffer_particles.frag
    ${SHADER_DIR}/deferred_gBuffer.vert
    ${SHADER_DIR}/deferred_render.frag
    ${SHADER_DIR}/FillTexture.comp
    ${SHADER_DIR}/floorShader.frag
    ${SHADER_DIR}/floorShader.vert
    ${SHADER_DIR}/particleShader.frag
    ${SHADER_DIR}/particleShader.vert
    ${SHADER_DIR}/particle_update.comp
    ${SHADER_DIR}/particle_bilateral_blur.comp
    ${SHADER_DIR}/particle_gbuffer_from_depth.comp
    ${SHADER_DIR}/PositionColor.vert
    ${SHADER_DIR}/quad.vert
    ${SHADER_DIR}/SolidColor.frag
)
set(SPIRV_FILES)
foreach(SHADER ${SHADERS})
    get_filename_component(SRC "${SHADER}" NAME)

    set(SPV ${SHADER_OUT_DIR}/${SRC}.spv)

    add_custom_command(
        OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/raw_${SRC}.spv
        COMMAND ${GLSLANG_EXE} -V --target-env vulkan1.0 ${SHADER} -o ${CMAKE_CURRENT_BINARY_DIR}/raw_${SRC}.spv
        DEPENDS ${SHADER}
        COMMENT "Compiling ${SRC} to SPIR-V"
        VERBATIM
    )
    add_custom_command(
        OUTPUT ${SPV}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${SHADER_OUT_DIR}
        COMMAND ${SPIRV_OPT_EXE} -O ${CMAKE_CURRENT_BINARY_DIR}/raw_${SRC}.spv -o ${SPV}
        DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/raw_${SRC}.spv
        COMMENT "Optimizing ${SRC}.spv"
        VERBATIM
    )

    list(APPEND SPIRV_FILES ${SPV})
endforeach()

add_custom_target(compile_shaders ALL DEPENDS ${SPIRV_FILES})

#  ========== PROJECT ==========

if (NOT WIN32 AND NOT MSVC)
    # windows cant stop complaining when compiling boreas
    # so we exclude it from windows builds
    add_subdirectory(boreas)
endif()

# Add warning flags for Windows and Linux
if (WIN32 OR MSVC)
add_compile_options(/EHsc /W4 /WX /wd4458 /wd4324)
else()
    add_compile_options(-Wall -Wextra -pedantic -Werror -Wconversion)
endif()

add_executable(app
    src/main.cpp
    src/camera.cpp
    src/controls/controls.cpp
    src/graphics/deferred_gbuffer_renderer.cpp
    src/graphics/deferred_lighting_renderer.cpp
    src/graphics/graphics.cpp
    src/graphics/imguisdl.cpp
    src/graphics/shaders.cpp
    src/physics/physics.cpp
)

add_dependencies(app compile_shaders)
target_link_libraries(app PRIVATE
    Eigen3::Eigen
    ImGui
    SDL3::SDL3
    UTL::include
)
target_compile_definitions(app PRIVATE
    SHADERS_DIR="${SHADER_OUT_DIR}/"
)

#  ========== TESTS ==========
