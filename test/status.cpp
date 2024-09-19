#include <gtest/gtest.h>

#include "process.hpp"

using namespace process;

TEST(StatusTest, CommandSuccess) {
  {
#ifdef _WIN32
    std::string bin = "cmd";
    std::vector<std::string> args{"/c", "echo hello"};
#else
    std::string bin = "sh";
    std::vector<std::string> args{"-c", "echo hello"};
#endif
    ExitStatus status = Command(bin).args(args).status();
    EXPECT_TRUE(status.success());
    EXPECT_EQ(status.code(), 0);
  }
  {
#ifdef _WIN32
    std::string bin = "cmd";
    std::vector<std::string> args{"/c", "ech hello"};
    // TODO:
#else
    std::string bin = "sh";
    std::vector<std::string> args{"-c", "ech hello"};
    ExitStatus status = Command(bin).args(args).status();
    EXPECT_FALSE(status.success());
    EXPECT_EQ(status.code(), 127);
#endif
  }
}

TEST(CmdTest, ExitCode) {
#ifdef _WIN32
  // EXPECT_THROW(Command("cmda").arg("/c").arg("echo hello"),
  // std::runtime_error);
#else
#endif
}
