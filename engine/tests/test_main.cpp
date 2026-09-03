#include "TestFramework.h"

namespace test {

std::vector<Case>& registry()
{
    static std::vector<Case> cases;
    return cases;
}

int failureCount = 0;

void reportFailure(const char* file, int line, const std::string& message)
{
    ++failureCount;
    std::printf("    %s:%d: %s\n", file, line, message.c_str());
}

} // namespace test

int main()
{
    int passed = 0;
    int failed = 0;

    for (const test::Case& c : test::registry())
    {
        const int before = test::failureCount;
        std::printf("  %s\n", c.name);
        c.fn();

        if (test::failureCount == before)
            ++passed;
        else
            ++failed;
    }

    std::printf("\n%d passed, %d failed (%d checks failed)\n",
                passed, failed, test::failureCount);

    return failed == 0 ? 0 : 1;
}
