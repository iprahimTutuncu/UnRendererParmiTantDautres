cmake_minimum_required(VERSION 3.31 FATAL_ERROR)
project(boreas)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_FLAGS "-O3")

set(OpenGL_GL_PREFERENCE GLVND)

find_package(SDL2 REQUIRED)
find_package(OpenGL 4.6 REQUIRED)
find_package(OpenMP)

add_compile_options(-march=native -mtune=native -mavx2 -O3)
add_executable(boreas
    ./libs/glad/glad.c
    ./src/core/ActionManager.cpp
    ./src/core/Application.cpp
    ./src/core/Window.cpp
    ./src/gfx/CameraPerspective.cpp
    ./src/gfx/FloorRenderer.cpp
    ./src/gfx/Mesh.cpp
    ./src/gfx/ParticleRenderer.cpp
    ./src/gfx/Renderer.cpp
    ./src/gfx/ShaderProgram.cpp
    ./src/Main.cpp
    ./src/mpm.cpp
)
target_include_directories(boreas PRIVATE libs/)

target_link_libraries(boreas
    Eigen3::Eigen
    glm::glm
    OpenGL::GL
    OpenMP::OpenMP_CXX
    SDL2::SDL2
    UTL::include
)
target_compile_definitions(boreas PRIVATE SHADERS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/shaders/")
