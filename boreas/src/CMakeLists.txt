# Add source files
set(SOURCE_FILES 
    Main.cpp 
    core/ActionManager.cpp
    core/Application.cpp
    core/Window.cpp
    gfx/CameraPerspective.cpp
    gfx/FloorRenderer.cpp
    gfx/Mesh.cpp
    gfx/ParticleRenderer.cpp
    gfx/Renderer.cpp
    gfx/ShaderProgram.cpp
    mpm/MpmSolver.cpp
)

set(HEADER_FILES 
    core/ActionManager.hpp
    core/Application.hpp
    core/Window.hpp
    gfx/CameraPerspective.hpp
    gfx/FloorRenderer.hpp
    gfx/Mesh.hpp
    gfx/ParticleRenderer.hpp
    gfx/Renderer.hpp
    gfx/ShaderProgram.hpp
    libs/eigen.hpp
    libs/sdl.hpp
    mpm/MpmGrid.hpp
    mpm/MpmGridNode.hpp
    mpm/MpmParticle.hpp
    mpm/MpmSolver.hpp
    mpm/MpmSolver.hpp
)

set(SHADER_FILES 
    shaders/particleShader.vert
    shaders/particleShader.frag
    shaders/floorShader.vert
    shaders/floorShader.frag
)

# Define the executable
add_executable(${PROJECT_NAME} ${HEADER_FILES} ${SOURCE_FILES} ${SHADER_FILES})

target_compile_definitions(${PROJECT_NAME} PUBLIC SHADERS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/shaders/")

target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(${PROJECT_NAME} 
    OpenGL::GL
    SDL2::SDL2
    Eigen3::Eigen
    GLEW::glew
    glm::glm
)
