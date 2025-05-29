cmake_minimum_required(VERSION 3.21 FATAL_ERROR)

project(Olaf
    VERSION 0.1
    LANGUAGES CXX
)

set(CMAKE_EXPORT_COMPILE_COMMANDS YES)

# Set build type
set(allowableBuildTypes Debug Release RelWithDebInfo MinSizeRel)
set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "${allowableBuildTypes}")
if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "" FORCE)
elseif (NOT CMAKE_BUILD_TYPE IN_LIST allowableBuildTypes)
    message(FATAL_ERROR "Invalid build type: ${CMAKE_BUILD_TYPE}")
endif()

if (MSVC)
    add_compile_options(/utf-8 /EHsc /W3)
else()
    add_compile_options(-W -Wall)
endif()

# go to the different modules
add_subdirectory(thirdParty)
add_subdirectory(Engine)
add_subdirectory(application)
