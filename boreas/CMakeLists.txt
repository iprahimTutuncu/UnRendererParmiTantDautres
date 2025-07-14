cmake_minimum_required(VERSION 3.31 FATAL_ERROR)
project(boreas)

set(OpenGL_GL_PREFERENCE GLVND)

find_package(SDL2 REQUIRED)
find_package(OpenGL 4.6 REQUIRED)
find_package(GLEW REQUIRED)
find_package(glm REQUIRED)
find_package(OpenMP)

add_executable(boreas
    src/Main.cpp
    src/core/ActionManager.cpp
    src/core/Application.cpp
    src/core/Window.cpp
    src/gfx/CameraPerspective.cpp
    src/gfx/FloorRenderer.cpp
    src/gfx/Mesh.cpp
    src/gfx/ParticleRenderer.cpp
    src/gfx/Renderer.cpp
    src/gfx/ShaderProgram.cpp
    src/mpm/MpmSolver.cpp
)

target_link_libraries(boreas
    OpenGL::GL
    SDL2::SDL2
    Eigen3::Eigen
    GLEW::glew
    glm::glm
    OpenMP::OpenMP_CXX
)
target_compile_definitions(boreas PRIVATE SHADERS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/shaders/")
