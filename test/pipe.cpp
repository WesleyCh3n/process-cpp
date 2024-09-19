#include <gtest/gtest.h>

#include "process.hpp"

using namespace process;
using std::string;
using std::vector;
using Args = vector<string>;

TEST(PipeTest, ProcessStdoutToProcessStdin) {
#ifdef _WIN32
#ifdef _DEBUG
  string bin = R"(Debug\mock.exe)";
#else
  string bin = R"(Release\mock.exe)";
#endif
#else
  string bin = "sh";
  Args args = {"-c", "echo hello world"};
#endif

  // FIX windos args insert first
  Args args = {bin, "out", "hello", "from", "out"};
  Child child = Command(bin).args(args).std_out(Stdio::pipe()).spawn();
  args = {bin, "in", "3"};
  Child child2 = Command(bin)
                     .args(args)
                     .std_in(Stdio::from(std::move(*child.io_stdout)))
                     .std_out(Stdio::pipe())
                     .std_err(Stdio::pipe())
                     .spawn();
  Output output = child2.wait_with_output();
  child.wait();

  EXPECT_STREQ(output.std_out.c_str(), "hellofromout");
  EXPECT_STREQ(output.std_err.c_str(), "");
  EXPECT_TRUE(output.status.success());
  EXPECT_EQ(output.status.code(), 0);
}
