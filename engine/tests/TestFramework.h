#pragma once

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

// A test runner small enough to vendor, so the engine builds and tests with
// nothing but a compiler and CMake -- no network fetch, no submodule.

namespace test {

using TestFn = void (*)();

struct Case
{
    const char* name;
    TestFn fn;
};

std::vector<Case>& registry();

struct Registrar
{
    Registrar(const char* name, TestFn fn) { registry().push_back({ name, fn }); }
};

extern int failureCount;

void reportFailure(const char* file, int line, const std::string& message);

} // namespace test

#define TEST(name)                                                             \
    static void name();                                                        \
    static ::test::Registrar registrar_##name(#name, name);                    \
    static void name()

#define CHECK(condition)                                                       \
    do {                                                                       \
        if (!(condition))                                                      \
            ::test::reportFailure(__FILE__, __LINE__, "CHECK(" #condition ")");\
    } while (false)

#define CHECK_MESSAGE(condition, message)                                      \
    do {                                                                       \
        if (!(condition))                                                      \
            ::test::reportFailure(__FILE__, __LINE__,                          \
                                  std::string("CHECK(" #condition ") -- ")     \
                                      + (message));                            \
    } while (false)

#define CHECK_NEAR(actual, expected, tolerance)                                \
    do {                                                                       \
        const double a_ = (double) (actual);                                   \
        const double e_ = (double) (expected);                                 \
        if (!(std::fabs(a_ - e_) <= (double) (tolerance)))                     \
        {                                                                      \
            char buf_[256];                                                    \
            std::snprintf(buf_, sizeof(buf_),                                  \
                          #actual " = %.9g, expected %.9g +/- %.9g",           \
                          a_, e_, (double) (tolerance));                       \
            ::test::reportFailure(__FILE__, __LINE__, buf_);                   \
        }                                                                      \
    } while (false)
