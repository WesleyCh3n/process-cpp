#include "../src/process.hpp"
#include <iomanip>
#include <iostream>

using namespace process;
int main() {
#ifndef _WIN32
  std::string app = "echo";
  std::vector<std::string> args{"hello", "world"};
#else
  std::string app = "cmd";
  std::vector<std::string> args{"/c", "echo hello world"};
#endif

  try {
    {
      auto child = Command(app).args(args).spawn();
      ExitStatus status = child.wait();

      std::cout << *status.code() << '\n';
      std::cout << std::boolalpha << status.success() << '\n';
    }
    {
      Child child = Command(app)
                        .args(args)
                        .std_out(Stdio::pipe())
                        .std_err(Stdio::pipe())
                        .spawn();
      Output output = child.wait_with_output();

      std::cout << std::quoted(output.std_out) << '\n';
      std::cout << std::quoted(output.std_err) << '\n';
    }
    {
      Output output = Command(app).args(args).output();

      std::cout << std::quoted(output.std_out) << '\n';
      std::cout << std::quoted(output.std_err) << '\n';
    }
  } catch (const std::exception &e) {
    std::cerr << e.what();
  }
}
