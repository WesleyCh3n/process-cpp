#include <gtest/gtest.h>

#include "process.hpp"

using namespace process;
using std::string;
using std::vector;
using Args = vector<string>;

TEST(OutputTest, Stdout) {
#ifdef _WIN32
#ifdef _DEBUG
  string bin = R"(Debug\mock.exe)";
#else
  string bin = R"(Release\mock.exe)";
#endif
  Args args = {"010", "\"hello from mock\""};
#else
  string bin = "sh";
  Args args = {"-c", "echo hello world"};
#endif

  Output output = Command(bin).args(args).output();
  EXPECT_STREQ(output.std_out.c_str(), "hello from mock");
  EXPECT_STREQ(output.std_err.c_str(), "");
  EXPECT_TRUE(output.status.success());
  EXPECT_EQ(output.status.code(), 0);
}

TEST(OutputTest, Stderr) {
#ifdef _WIN32
#ifdef _DEBUG
  string bin = R"(Debug\mock.exe)";
#else
  string bin = R"(Release\mock.exe)";
#endif
  Args args = {"100", "\"hello from mock\""};
#else
  string bin = "sh";
  Args args = {"-c", "echo hello world"};
#endif

  Output output = Command(bin).args(args).output();
  EXPECT_STREQ(output.std_out.c_str(), "");
  EXPECT_STREQ(output.std_err.c_str(), "hello from mock");
  EXPECT_TRUE(output.status.success());
  EXPECT_EQ(output.status.code(), 0);
}

TEST(OutputTest, StdoutStderr) {
#ifdef _WIN32
#ifdef _DEBUG
  string bin = R"(Debug\mock.exe)";
#else
  string bin = R"(Release\mock.exe)";
#endif
  Args args = {"110", "\"stdout from mock\"", "\"stderr from mock\""};
#else
  string bin = "sh";
  Args args = {"-c", "echo hello world"};
#endif

  Output output = Command(bin).args(args).output();
  EXPECT_STREQ(output.std_out.c_str(), "stdout from mock");
  EXPECT_STREQ(output.std_err.c_str(), "stderr from mock");
  EXPECT_TRUE(output.status.success());
  EXPECT_EQ(output.status.code(), 0);
}
