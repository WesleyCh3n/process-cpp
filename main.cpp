#include "process.hpp"
#include <iomanip>
#include <iostream>
#include <thread>

int main(int argc, char *argv[]) {
  {
    auto child = process::Command().arg("ls -lh").spawn();
    child.wait();
  }
  {
    auto child = process::Command()
                     .arg(argv[1])
                     .std_out(process::Stdio::pipe())
                     .std_err(process::Stdio::pipe())
                     .spawn();
    std::cout << "pid: " << child.id() << '\n';
    std::this_thread::sleep_for(std::chrono::seconds(3));
    char buffer[2048];
    ssize_t size = 0;

    if (child.io_stdout.has_value())
      while ((size = child.io_stdout->read(buffer, 2048)) > 0) {
        std::cout << "[OUT]: " << std::quoted(buffer) << '\n';
      }
    if (child.io_stderr.has_value())
      while ((size = child.io_stderr->read(buffer, 2048)) > 0) {
        std::cout << "[ERR]: " << std::quoted(buffer) << '\n';
      }
    child.wait();
  }
}
