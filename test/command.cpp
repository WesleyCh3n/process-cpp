#include <gtest/gtest.h>

#include "process.hpp"

using namespace process;
using std::string;
using std::vector;
using Args = vector<string>;

TEST(CmdTest, CommandShell) {
#ifdef _WIN32
  string bin = "cmd";
  Args args = {"/c", "echo hello world"};
#else
  string bin = "sh";
  Args args = {"-c", "echo hello world"};
#endif
  ExitStatus status = Command(bin).args(args).status();
  EXPECT_TRUE(status.success());
  EXPECT_EQ(status.code(), 0);
}

TEST(CmdTest, CommandFullPath) {
#ifdef _WIN32
  string bin = R"(C:\WINDOWS\system32\cmd.exe)";
  Args args = {"/c", "echo hello world"};
#else
  string bin = "/bin/sh";
  Args args = {"-c", "echo hello world"};
#endif
  ExitStatus status = Command(bin).args(args).status();
  EXPECT_TRUE(status.success());
  EXPECT_EQ(status.code(), 0);
}

TEST(CmdTest, CommandNotFound) {
#ifdef _WIN32
  string bin = "not_exist";
#else
  string bin = "not_exist";
#endif
  EXPECT_THROW(Command(bin).spawn(), std::runtime_error);
}
