#include "src/process.hpp"

#include <iomanip>
#include <iostream>

int main(int argc, char *argv[]) {
  try {
    {
      auto status = process::Command()
                        .arg(argv[1])
                        // .current_dir(R"(C:\AMD)")
                        // .env("set", "2")
                        .status();
      std::cout << std::boolalpha << status.success() << std::endl;
      std::cout << *status.code() << std::endl;
    }
    {
      auto output = process::Command()
                        .arg(argv[1])
                        // .current_dir(R"(C:\AMD)")
                        .env("ver", "1")
                        .env("PATH", "")
                        // .env("set", "2")
                        .output();
      std::cout << std::boolalpha << output.status.success() << std::endl;
      std::cout << std::quoted(output.std_out) << std::endl;
      std::cout << std::quoted(output.std_err) << std::endl;
    }
    /* {
      auto child = process::Command(R"(.\build\Debug\test.exe)").spawn();
      std::cout << "pid: " << child.id() << '\n';
      child.wait();
    }
    std::cout << std::string(80, '=') << std::endl;
    {
      auto child = process::Command(R"(.\build\Debug\test.exe)")
                       .std_out(process::Stdio::pipe())
                       .std_err(process::Stdio::pipe())
                       .spawn();
      std::cout << "pid: " << child.id() << '\n';
      char buffer[2048];
      ssize_t size = 0;
      if (child.io_stdout.has_value())
        while ((size = child.io_stdout->read(buffer, 2048)) > 0) {
          std::cout << "[OUT]: " << std::quoted(buffer) << '\n';
        }
      if (child.io_stderr.has_value()) {
        while ((size = child.io_stderr->read(buffer, 2048)) > 0) {
          std::cout << "[ERR]: " << std::quoted(buffer) << '\n';
        }
      }
      child.wait();
    }
    std::cout << std::string(80, '=') << std::endl;
    {
      auto child = process::Command()
                       .arg(argv[1])
                       .std_out(process::Stdio::pipe())
                       .std_err(process::Stdio::pipe())
                       .spawn();
      std::cout << "pid: " << child.id() << '\n';
      // std::this_thread::sleep_for(std::chrono::seconds(3));
      char buffer[2048];
      ssize_t size = 0;
      if (child.io_stdout.has_value())
        while ((size = child.io_stdout->read(buffer, 2048)) > 0) {
          std::cout << "[OUT]: " << std::quoted(buffer) << '\n';
        }
      if (child.io_stderr.has_value()) {
        while ((size = child.io_stderr->read(buffer, 2048)) > 0) {
          std::cout << "[ERR]: " << std::quoted(buffer) << '\n';
        }
      }
      child.wait();
    }
    std::cout << std::string(80, '=') << std::endl; */
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
  }
}
