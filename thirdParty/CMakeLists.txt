if (WIN32)
    # SDL3
    add_library(SDL3::SDL3 STATIC IMPORTED GLOBAL)
    set_target_properties(SDL3::SDL3 PROPERTIES
        IMPORTED_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/lib/SDL3.lib"
        INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/include"
    )

    # GLEW
    add_library(GLEW::GLEW STATIC IMPORTED GLOBAL)
    set_target_properties(GLEW::GLEW PROPERTIES
        IMPORTED_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/lib/glew32.lib"
        INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/include"
    )

    # spdlog
    add_library(spdlog::spdlog INTERFACE IMPORTED GLOBAL)
    target_include_directories(spdlog::spdlog INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/spdlog>
        $<INSTALL_INTERFACE:include>
    )

    # opengl
    add_library(OpenGL::GL INTERFACE IMPORTED GLOBAL)
    set_target_properties(OpenGL::GL PROPERTIES
        IMPORTED_LOCATION opengl32.lib
    )

    # glm
    add_library(glm::glm INTERFACE IMPORTED GLOBAL)
    target_include_directories(glm::glm INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/glm>
        $<INSTALL_INTERFACE:include>
    )

else()
    find_package(SDL3 REQUIRED GLOBAL)
    find_package(GLEW REQUIRED GLOBAL)
    find_package(spdlog REQUIRED GLOBAL)
    find_package(glm REQUIRED GLOBAL)
    find_package(OpenGL 4.5 REQUIRED GLOBAL)
endif()

# ImGUI
add_library(ImGui STATIC 
    include/imgui/imgui.cpp
    include/imgui/imgui_draw.cpp
    include/imgui/imgui_tables.cpp
    include/imgui/imgui_widgets.cpp
    include/imgui/imgui_demo.cpp
    include/imgui/backends/imgui_impl_sdl3.cpp
    include/imgui/backends/imgui_impl_sdlgpu3.cpp
)

target_link_libraries(ImGui
    PRIVATE
        SDL3::SDL3
        OpenGL::GL
)

target_include_directories(ImGui
    PUBLIC 
        include/imgui
)
