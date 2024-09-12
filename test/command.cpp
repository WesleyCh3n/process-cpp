#include <gtest/gtest.h>

#include "process.hpp"

using namespace process;

TEST(CmdTest, CommandShell) {
#ifdef _WIN32
  ExitStatus status = Command("cmd").arg("/c").arg("echo hello").status();
  EXPECT_TRUE(status.success());
  EXPECT_EQ(status.code(), 0);
#else
#endif
}

TEST(CmdTest, CommandFullPath) {
#ifdef _WIN32
  ExitStatus status = Command(R"(C:\WINDOWS\system32\cmd.exe)")
                          .arg("/c")
                          .arg("echo hello")
                          .status();
  EXPECT_TRUE(status.success());
  EXPECT_EQ(status.code(), 0);
#else
#endif
}

TEST(CmdTest, CommandNotFound) {
#ifdef _WIN32
  EXPECT_THROW(
      {
        try {
          Command("cmda").arg("/c").arg("echo hello");
        } catch (const std::runtime_error &e) {
          // and this tests that it has the correct message
          EXPECT_STREQ("executable not found: cmda.exe", e.what());
          // EXPECT_THAT(e.what(), Not(HasSubstr("executable not found:")));
          throw;
        }
      },
      std::runtime_error);
#else
#endif
}

// TEST(CmdTest, FailedCommand) {
// #ifdef _WIN32
// #else
// #endif
// }
