#include "src/process.hpp"

#include <iomanip>
#include <iostream>

using namespace std;

int main(int argc, char *argv[]) {
  try {
    using namespace process;
    {
      auto child = Command("./build/getline").spawn();
      std::cout << "pid: " << child.id() << '\n';
      child.kill();
      std::cout << "kill success\n";
    }
    std::cout << std::string(80, '=') << std::endl;
    {
      auto child = Command("./build/getline").std_in(Stdio::pipe()).spawn();
      vector<string> strs{"hello\n", "from\n", "parent\n"};
      for (auto &str : strs) {
        child.io_stdin->write(std::as_bytes(std::span{str}));
      }
      child.wait();
    }
    std::cout << std::string(80, '=') << std::endl;
    {
      auto output = Command("echo").arg("hello").output();
      cout << "exit code: " << *output.status.code() << '\n';
      cout << "out: " << output.std_out << '\n';
      cout << "err: " << output.std_err << '\n';
    }
    std::cout << std::string(80, '=') << std::endl;
    {
      auto child =
          Command("cat").arg("main.cpp").std_out(Stdio::pipe()).spawn();
      auto output = Command("grep")
                        .arg("./build/getline")
                        .std_in(Stdio::from(std::move(*child.io_stdout)))
                        .output();
      cout << "exit code: " << *output.status.code() << '\n';
      cout << "out: " << output.std_out << '\n';
      cout << "err: " << output.std_err << '\n';
    }
    std::cout << std::string(80, '=') << std::endl;
    {
      auto child = process::Command("ls")
                       .std_out(process::Stdio::pipe())
                       .std_err(process::Stdio::pipe())
                       .spawn();
      std::cout << "pid: " << child.id() << '\n';
      string buffer(2048, '\0');
      ssize_t size = 0;
      if (child.io_stdout.has_value())
        while ((size = child.io_stdout->read(
                    std::as_writable_bytes(span{buffer}))) > 0) {
          std::cout << "[OUT]: " << std::quoted(buffer) << '\n';
        }
      if (child.io_stderr.has_value()) {
        while ((size = child.io_stderr->read(
                    std::as_writable_bytes(span{buffer}))) > 0) {
          std::cout << "[ERR]: " << std::quoted(buffer) << '\n';
        }
      }
      child.wait();
    }
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
  }
}
