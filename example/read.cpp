#include "../src/process.hpp"
#include <cassert>
#include <iomanip>
#include <iostream>

using namespace process;
int main() {
#ifndef _WIN32
  std::string app = "ls";
  std::vector<std::string> args{};
#else
  std::string app = "cmd";
  std::vector<std::string> args{"/c", "dir"};
#endif

  try {
    {
      auto child = Command(app)
                       .args(args)
                       .std_out(Stdio::pipe())
                       .std_err(Stdio::pipe())
                       .spawn();
      std::cout << child.id() << '\n';
      assert(child.io_stdout);
      assert(child.io_stderr);
      std::string out, err;
      child.io_stdout->read_to_string(out);
      child.io_stderr->read_to_string(err);
      ExitStatus status = child.wait();

      std::cout << *status.code() << '\n';
      std::cout << std::quoted(out) << '\n';
      std::cout << std::quoted(err) << '\n';
    }
    {
      auto output = Command(app)
                        .args(args)
                        .std_out(Stdio::null()) // pipe to null
                        .output();
      std::cout << *output.status.code() << '\n';
      std::cout << std::quoted(output.std_out) << '\n';
      std::cout << std::quoted(output.std_err) << '\n';
    }
  } catch (const std::exception &e) {
    std::cerr << e.what();
  }
}
