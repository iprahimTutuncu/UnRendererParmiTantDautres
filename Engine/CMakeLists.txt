add_library(OlafEngine STATIC
    src/engine.cpp
    src/graphics/graphic_manager.cpp
    src/graphics/graphic_api.cpp
    src/platform/system/window_sdl3.cpp
    src/system/keyboard.cpp
    src/system/log_manager.cpp
    src/system/window.cpp
    src/system/control_setting.cpp
    src/system/system_manager.cpp
)

target_compile_features(OlafEngine
    PRIVATE
        cxx_std_20
)

target_precompile_headers(OlafEngine
    PRIVATE
        src/pch.h
)

target_include_directories(OlafEngine
    PUBLIC
        include
    PRIVATE
        src
)

target_link_libraries(OlafEngine
    PRIVATE
        SDL3::SDL3
        OpenGL::GL
        spdlog::spdlog
        glm::glm
        GLEW::GLEW
)
