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

# spdlog: Fast C++ logging library
set(SPDLOG_ENABLE_PCH ON)
set(SPDLOG_BUILD_PIC ON)
add_subdirectory(libs/spdlog)

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

add_custom_target(generate_stb_wrapper ALL DEPENDS ${SPIRV_FILES})
add_library(stb STATIC ${GENERATED_STB_WRAPPER})
target_include_directories(stb INTERFACE libs/stb)
target_compile_definitions(stb PRIVATE
    STB_IMAGE_IMPLEMENTATION
)


# OpenGL Mathematics (GLM)
add_subdirectory(libs/glm)


#  ========== SHADERS ==========

set(SHADER_DIR ${CMAKE_CURRENT_SOURCE_DIR}/shaders)
set(SHADER_OUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/shaders)

file(GLOB SHADERS "${SHADER_DIR}/*")
set(SPIRV_FILES)
foreach(SHADER ${SHADERS})
    get_filename_component(SRC "${SHADER}" NAME)

    set(SPV ${SHADER_OUT_DIR}/${SRC}.spv)

    add_custom_command(
        OUTPUT ${SPV}
        COMMAND glslang ${SHADER} --client vulkan1.3 -V -o ${SPV}
        DEPENDS ${SHADER}
        COMMENT "Compiling ${SHADER_DIR}/${SRC} to SPIR-V"
    )

    list(APPEND SPIRV_FILES ${SPV})
endforeach()

add_custom_target(compile_shaders ALL DEPENDS ${SPIRV_FILES})

#  ========== PROJECT ==========

# Add warning flags for Windows and Linux
if (WIN32 OR MSVC)
    add_compile_options(/EHsc /W4 /WX /wd4458 /wd4324)
else()
    add_compile_options(-Wall -Wextra -pedantic -Werror)
endif()

if (NOT (CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "MinSizeRel"))
    add_compile_definitions(DEBUG=1)
endif()

# go to the different modules
add_library(GTS STATIC
    Engine/src/vendor/common.cpp
    Engine/src/platform/system/window_sdl3.cpp
    Engine/src/system/log_manager.cpp
    Engine/src/system/system_manager.cpp
    Engine/src/system/control_setting.cpp
    Engine/src/system/keyboard.cpp
    Engine/src/system/window.cpp
    Engine/src/ressource_manager/image_manager.cpp
    Engine/src/ressource_manager/shader_manager.cpp
    Engine/src/ressource_manager/texture_manager.cpp
    Engine/src/ressource_manager/sampler_manager.cpp
    Engine/src/engine.cpp
    Engine/src/graphics/deferred_gbuffer_renderer.cpp
    Engine/src/graphics/graphic_manager.cpp
    Engine/src/graphics/graphic_api.cpp
    Engine/src/graphics/image.cpp
    Engine/src/graphics/deferred_lighting_renderer.cpp
)
target_include_directories(GTS INTERFACE Engine/src)
target_link_libraries(GTS PRIVATE
    SDL3::SDL3
    spdlog::spdlog
    stb
    glm
)
add_subdirectory(application)

add_executable(app
    src/main.cpp
    src/camera.cpp
    src/controls/controls.cpp
    src/graphics/graphics.cpp
    src/physics/physics.cpp
)
target_link_libraries(app PRIVATE
    SDL3::SDL3
)
target_compile_definitions(app PRIVATE
    SHADERS_DIR="${SHADER_OUT_DIR}/"
)

#  ========== TESTS ==========
