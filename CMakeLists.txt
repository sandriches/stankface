cmake_minimum_required(VERSION 3.22)

project(StankfaceSynth VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

option(STANKFACE_BUILD_TESTS "Build the engine test suite" ON)
option(STANKFACE_BUILD_TOOLS "Build the offline render tool" ON)

# The DSP core. No JUCE, no host framework, no platform headers -- the point of
# keeping it separate is that a wrapper for any host can link it unchanged.
add_subdirectory(engine)

if(STANKFACE_BUILD_TESTS)
    enable_testing()
    add_subdirectory(engine/tests)
endif()

if(STANKFACE_BUILD_TOOLS)
    add_subdirectory(tools)
endif()
