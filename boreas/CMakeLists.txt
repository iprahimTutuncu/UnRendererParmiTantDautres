cmake_minimum_required(VERSION 3.31 FATAL_ERROR)
project(boreas)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_FLAGS "-O3")

set(OpenGL_GL_PREFERENCE GLVND)

find_package(SDL2 REQUIRED)
find_package(OpenGL 4.6 REQUIRED)
find_package(OpenMP)

add_compile_options(-march=native -mtune=native -mavx2)
add_executable(boreas
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
    ./src/mpm/MpmSolver.cpp
    ./libs/glad/glad.c
)
target_include_directories(boreas PRIVATE libs/)

target_link_libraries(boreas
    OpenGL::GL
    SDL2::SDL2
    Eigen3::Eigen
    glm::glm
    # OpenMP::OpenMP_CXX
)
target_compile_definitions(boreas PRIVATE SHADERS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/shaders/")
add_compile_options(-Wall -Wextra -pedantic -Werror -Wconversion)
