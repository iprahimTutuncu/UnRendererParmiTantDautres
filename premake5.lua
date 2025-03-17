-- Root solution configuration
workspace "UnRendererParmiTantDautres"
    location "."  -- This places the solution in the root directory
    configurations { "Debug", "Release" }
    platforms { "Win64" }

    filter { "platforms:Win64" }
        system "Windows"
        architecture "x86_64"
        defines { "PLATFORM_WINDOWS" }

-- Path to third-party dependencies
local thirdPartyPath = "thirdParty"
local includePath = thirdPartyPath .. "/include"
local libPath = thirdPartyPath .. "/lib"

-- Engine static library project
project "Engine"
    location "Engine"  -- The project files are now in the "Engine" folder
    kind "StaticLib"
    language "C++"
    cppdialect "C++20"
    targetdir "bin/%{cfg.buildcfg}"
    objdir "obj/%{cfg.buildcfg}/%{prj.name}"

    includedirs {
        "Engine/include", 
        includePath,
        includePath .. "/imgui"
    }
    
    libdirs {
        libPath,
    }

    files {
        "Engine/**.h",
        "Engine/**.cpp",

        -- ImGui source files
        includePath .. "/imgui/imgui.cpp",
        includePath .. "/imgui/imgui_tables.cpp",
        includePath .. "/imgui/imgui_demo.cpp",
        includePath .. "/imgui/imgui_draw.cpp",
        includePath .. "/imgui/imgui_widgets.cpp",

        -- ImGui SDL3 backend
        includePath .. "/imgui/backends/imgui_impl_sdl3.cpp",
        includePath .. "/imgui/backends/imgui_impl_sdlgpu3.cpp",
        includePath .. "/imgui/backends/imgui_impl_sdlrenderer3.cpp",
        -- ImGui OpenGL3 backend
        includePath .. "/imgui/backends/imgui_impl_opengl3.cpp",
        -- ImGui Vulkan backend
        includePath .. "/imgui/backends/imgui_impl_vulkan.cpp",
        -- ImGui Vulkan bootstrap
        includePath .. "/vk_bootstrap/VkBootstrap.cpp"
    }
	
	buildoptions { "/utf-8" }

    -- Precompiled header settings
    filter "action:vs*"
        pchheader "pch.h"
        pchsource "Engine/src/pch.cpp"
		
    filter "action:not vs*"
        -- For other toolchains (like GCC/Clang), set PCH header but don't specify compiler flags
        pchheader "Engine/include/pch.h"
    
    filter "configurations:Debug"
        defines { "DEBUG" }
        symbols "On"

    filter "configurations:Release"
        defines { "NDEBUG" }
        optimize "On"


-- Application project
project "Application"
    location "Application"  -- The project files are now in the "Application" folder
    kind "ConsoleApp"
    language "C++"
    cppdialect "C++20"
    targetdir "bin/%{cfg.buildcfg}"

	buildoptions { "/utf-8" }
	
    includedirs {
        "Engine/include",
        includePath
    }

    libdirs {
        libPath
    }

    files {
        "Application/**.h",
        "Application/**.cpp"
    }
    
    links { "Engine" }
    links { "SDL3", "vulkan-1", "glew32", "opengl32" }

    -- Precompiled header settings
    filter "action:vs*"
        pchheader "pch.h"
        pchsource "Application/src/pch.cpp"
		
    filter "action:not vs*"
        -- For other toolchains (like GCC/Clang), set PCH header but don't specify compiler flags
        pchheader "Application/src/pch.h"
    
    filter "configurations:Debug"
        defines { "DEBUG" }
        symbols "On"

    filter "configurations:Release"
        defines { "NDEBUG" }
        optimize "On"
