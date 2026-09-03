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

# Off by default: the plugin pulls JUCE down over the network on first
# configure, which is not something a plain engine build should do unasked.
option(STANKFACE_BUILD_PLUGIN "Build the JUCE plugin wrapper" OFF)

# Off by default: this writes into the system plug-in folders.
option(STANKFACE_INSTALL_PLUGIN "Copy the built plugin into the system plug-in folders" OFF)

set(STANKFACE_JUCE_TAG "8.0.15" CACHE STRING "JUCE release tag to build against")
set(STANKFACE_PLUGIN_FORMATS "AU;VST3;Standalone" CACHE STRING "Plugin formats to build")

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

if(STANKFACE_BUILD_PLUGIN)
    # JUCE needs these, and on Apple the platform layer is Objective-C++. They
    # are enabled here rather than in project() so that an engine-only build
    # does not demand toolchains it never uses.
    enable_language(C)
    if(APPLE)
        enable_language(OBJC)
        enable_language(OBJCXX)
    endif()

    add_subdirectory(wrappers/juce-plugin)
endif()
