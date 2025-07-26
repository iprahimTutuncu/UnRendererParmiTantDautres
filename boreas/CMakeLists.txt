cmake_minimum_required(VERSION 3.31 FATAL_ERROR)
project(boreas)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_FLAGS "-O3")

set(OpenGL_GL_PREFERENCE GLVND)

find_package(SDL2 REQUIRED)
find_package(OpenGL 4.6 REQUIRED)
find_package(GLEW REQUIRED)
find_package(glm REQUIRED)
find_package(OpenMP)


set(EIGEN_BUILD_TESTING OFF)
set(EIGEN_INCLUDE_INSTALL_DIR OFF)
add_subdirectory(eigen EXCLUDE_FROM_ALL)

add_subdirectory(src)
