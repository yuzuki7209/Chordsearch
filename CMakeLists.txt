cmake_minimum_required(VERSION 3.22)

set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")

# Windows: static C++ runtime so testers don't need the VC++ redistributable
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

project(Chordsearch VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(MSVC)
    # Sources contain UTF-8 (Korean UI text, ø/° in roman numerals)
    add_compile_options(/utf-8)
endif()

option(CHORDSEARCH_BUILD_TESTS "Build unit tests for the core engine" ON)

add_subdirectory(external/JUCE)
add_subdirectory(core)
add_subdirectory(plugin)

if(CHORDSEARCH_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

option(CHORDSEARCH_BUILD_TOOLS "Build developer tools (UI snapshot renderer)" ON)
if(CHORDSEARCH_BUILD_TOOLS)
    add_subdirectory(tools/ui_snapshot)
endif()
