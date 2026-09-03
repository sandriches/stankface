add_library(stankface_engine STATIC
    src/WavetableData.gen.cpp
    src/WavetableOscillator.cpp
    src/Filter.cpp
    src/Envelope.cpp
    src/Lfo.cpp
    src/Params.cpp
    src/WavetableEngine.cpp
)

target_include_directories(stankface_engine PUBLIC include)

target_compile_options(stankface_engine PRIVATE
    $<$<CXX_COMPILER_ID:AppleClang,Clang,GNU>:-Wall -Wextra -Wpedantic>
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
)
