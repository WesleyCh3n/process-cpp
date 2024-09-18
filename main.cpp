#include "src/process.hpp"

#include <windows.h>

#include <iostream>

using namespace std;

int main(int argc, char *argv[]) {
  try {
    using namespace process;
    {
      // auto child = Command("cmd.exe")
      //                  .arg("/c")
      //                  .arg("echo hello")
      //                  .std_out(Stdio::pipe())
      //                  .spawn();
      auto output = Command("cmd")
                        .arg("/c")
                        .arg("echo hello | findstr h")
                        // .std_in(Stdio::from(std::move(*child.io_stdout)))
                        .output();
      // output.wait();
      cout << *output.status.code() << '\n';
      cout << "out: " << output.std_out << '\n';
      cout << "err: " << output.std_err << '\n';
      // child.wait();
    }
    {
      auto child = Command("cmd")
                       .arg("/c")
                       .arg(R"(ping 127.0.0.1)")
                       .std_out(Stdio::pipe())
                       .spawn();
      auto output = Command("cmd")
                        .arg("/c")
                        .arg(R"(.\build\Debug\test.exe)")
                        .std_in(Stdio::from(std::move(*child.io_stdout)))
                        .output();
      cout << *output.status.code() << '\n';
      cout << "out: " << output.std_out << '\n';
      cout << "err: " << output.std_err << '\n';
      // child.wait();
    }
    /* std::cout << std::string(80, '=') << std::endl;
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
