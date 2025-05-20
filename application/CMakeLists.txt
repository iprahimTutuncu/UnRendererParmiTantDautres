add_executable(OlafApplication
    main.cpp
)
target_compile_features(OlafApplication
    PRIVATE
        cxx_std_20
)

target_precompile_headers(OlafApplication
    PRIVATE
        pch.h
)

target_include_directories(OlafApplication
    PRIVATE
        ./
)

target_link_libraries(OlafApplication
    PRIVATE
        OlafEngine
        ImGui
)

# copy SDL DLLs if on Windows
if (WIN32)
    add_custom_command(TARGET OlafApplication POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/bin-dep/SDL3.dll"
            $<TARGET_FILE_DIR:OlafApplication>
        COMMAND_EXPAND_LISTS
    )
endif()
