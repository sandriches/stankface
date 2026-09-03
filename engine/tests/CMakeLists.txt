add_executable(engine_tests
    test_main.cpp
    test_oscillator.cpp
    test_envelope.cpp
    test_filter.cpp
    test_engine.cpp
)

target_link_libraries(engine_tests PRIVATE stankface_engine)
target_include_directories(engine_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

add_test(NAME engine_tests COMMAND engine_tests)
