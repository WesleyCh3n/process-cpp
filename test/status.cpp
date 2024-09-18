#include <gtest/gtest.h>

#include "process.hpp"

using namespace process;

TEST(StatusTest, CommandSuccess) {
#ifdef _WIN32
  {
    ExitStatus status = Command("cmd").arg("/c").arg("echo hello").status();
    EXPECT_TRUE(status.success());
    EXPECT_EQ(status.code(), 0);
  }
  {
    ExitStatus status = Command("cmd").arg("/c").arg("ech hello").status();
    EXPECT_FALSE(status.success());
    EXPECT_EQ(status.code(), 1);
  }
#else
#endif
}

TEST(CmdTest, ExitCode) {
#ifdef _WIN32
  // EXPECT_THROW(Command("cmda").arg("/c").arg("echo hello"),
  // std::runtime_error);
#else
#endif
}
