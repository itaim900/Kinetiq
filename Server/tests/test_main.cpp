#include <gtest/gtest.h>

#include <cstdlib>

#include "config/Env.h"

TEST(EnvTest, ReturnsFallbackWhenVariableUnset) {
    EXPECT_EQ(kinetiq::envOr("KINETIQ_DEFINITELY_UNSET_VAR_XYZ", "fallback"), "fallback");
}

TEST(EnvTest, ReturnsValueWhenVariableSet) {
#if defined(_WIN32)
    _putenv_s("KINETIQ_TEST_VAR", "hello");
#else
    setenv("KINETIQ_TEST_VAR", "hello", 1);
#endif
    EXPECT_EQ(kinetiq::envOr("KINETIQ_TEST_VAR", "fallback"), "hello");
}
