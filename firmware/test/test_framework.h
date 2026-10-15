#ifndef VOLTWATCH_TEST_FRAMEWORK_H
#define VOLTWATCH_TEST_FRAMEWORK_H

#include <iostream>
#include <string>
#include <cmath>
#include <vector>
#include <functional>

#include <sstream>

namespace VoltWatchTest {

template <typename T>
inline std::string stringify(const T& val) {
    std::ostringstream oss;
    oss << val;
    return oss.str();
}

struct TestCase {
    std::string name;
    std::string suite;
    std::function<void()> func;
};

class TestRunner {
public:
    static TestRunner& instance() {
        static TestRunner s_runner;
        return s_runner;
    }

    void registerTest(const std::string& suite, const std::string& name, std::function<void()> func) {
        m_tests.push_back({name, suite, func});
    }

    int runAll() {
        int passed = 0;
        int failed = 0;

        std::cout << "\033[1;36m====================================================\033[0m\n";
        std::cout << "\033[1;36m           VoltWatch Native Unit Test Suite         \033[0m\n";
        std::cout << "\033[1;36m====================================================\033[0m\n";

        for (const auto& test : m_tests) {
            std::cout << "[ RUN      ] " << test.suite << "::" << test.name << "\n";
            m_currentTestPassed = true;
            m_currentFailureReason.clear();

            try {
                test.func();
            } catch (const std::exception& ex) {
                m_currentTestPassed = false;
                m_currentFailureReason = std::string("Unhandled exception: ") + ex.what();
            } catch (...) {
                m_currentTestPassed = false;
                m_currentFailureReason = "Unknown non-standard exception thrown";
            }

            if (m_currentTestPassed) {
                std::cout << "\033[1;32m[       OK ]\033[0m " << test.suite << "::" << test.name << "\n";
                passed++;
            } else {
                std::cout << "\033[1;31m[  FAILED  ]\033[0m " << test.suite << "::" << test.name << "\n";
                if (!m_currentFailureReason.empty()) {
                    std::cout << "             Reason: " << m_currentFailureReason << "\n";
                }
                failed++;
            }
        }

        std::cout << "\033[1;36m====================================================\033[0m\n";
        std::cout << "Test Summary: Total: " << m_tests.size()
                  << " | \033[1;32mPassed: " << passed << "\033[0m"
                  << " | \033[1;" << (failed > 0 ? "31" : "32") << "mFailed: " << failed << "\033[0m\n";
        std::cout << "\033[1;36m====================================================\033[0m\n";

        return failed == 0 ? 0 : 1;
    }

    void recordFailure(const std::string& reason) {
        m_currentTestPassed = false;
        m_currentFailureReason = reason;
    }

private:
    std::vector<TestCase> m_tests;
    bool m_currentTestPassed{true};
    std::string m_currentFailureReason;
};

#define TEST_CASE(suite_name, test_name) \
    static void test_##suite_name##_##test_name(); \
    namespace { \
        struct Reg_##suite_name##_##test_name { \
            Reg_##suite_name##_##test_name() { \
                VoltWatchTest::TestRunner::instance().registerTest(#suite_name, #test_name, test_##suite_name##_##test_name); \
            } \
        } global_reg_##suite_name##_##test_name; \
    } \
    static void test_##suite_name##_##test_name()

#define ASSERT_TRUE(condition) \
    do { \
        if (!(condition)) { \
            VoltWatchTest::TestRunner::instance().recordFailure( \
                std::string("Assertion failed: ") + #condition + " at " + __FILE__ + ":" + std::to_string(__LINE__)); \
            return; \
        } \
    } while(0)

#define ASSERT_FALSE(condition) \
    do { \
        if (condition) { \
            VoltWatchTest::TestRunner::instance().recordFailure( \
                std::string("Assertion failed (expected false): ") + #condition + " at " + __FILE__ + ":" + std::to_string(__LINE__)); \
            return; \
        } \
    } while(0)

#define ASSERT_EQ(expected, actual) \
    do { \
        if ((expected) != (actual)) { \
            VoltWatchTest::TestRunner::instance().recordFailure( \
                std::string("Expected: ") + VoltWatchTest::stringify(expected) + ", Actual: " + VoltWatchTest::stringify(actual) + " at " + __FILE__ + ":" + std::to_string(__LINE__)); \
            return; \
        } \
    } while(0)

#define ASSERT_FLOAT_NEAR(expected, actual, epsilon) \
    do { \
        if (std::fabs((expected) - (actual)) > (epsilon)) { \
            VoltWatchTest::TestRunner::instance().recordFailure( \
                std::string("Float near mismatch: Expected: ") + std::to_string(expected) + ", Actual: " + std::to_string(actual) + " (diff > " + std::to_string(epsilon) + ") at " + __FILE__ + ":" + std::to_string(__LINE__)); \
            return; \
        } \
    } while(0)

} // namespace VoltWatchTest

#endif // VOLTWATCH_TEST_FRAMEWORK_H
