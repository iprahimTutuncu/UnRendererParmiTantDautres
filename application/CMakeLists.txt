add_executable(OlafApplication
    main.cpp
    imgui/ImGuiSDLGPU.cpp
)
target_compile_features(OlafApplication
    PRIVATE
        cxx_std_23
)

target_include_directories(OlafApplication
    PRIVATE
        ./
)

target_link_libraries(OlafApplication
    PRIVATE
        GTS
        ImGui
)

