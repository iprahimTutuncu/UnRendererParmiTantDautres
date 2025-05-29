add_library(OlafEngine STATIC
    src/engine.cpp
    src/graphics/graphic_api.cpp
    src/graphics/graphic_manager.cpp
    src/platform/system/window_sdl3.cpp
    src/system/control_setting.cpp
    src/system/keyboard.cpp
    src/system/log_manager.cpp
    src/system/system_manager.cpp
    src/system/window.cpp
)

target_compile_features(OlafEngine
    PRIVATE
        cxx_std_23
)


target_include_directories(OlafEngine PUBLIC
    include
)

target_link_libraries(OlafEngine
    PRIVATE
        SDL3::SDL3
        OpenGL::GL
        spdlog::spdlog
        glm::glm-header-only
)
