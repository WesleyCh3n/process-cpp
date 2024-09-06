#include "process.hpp"

int main(int argc, char *argv[]) {
  {
    auto cmd = process::Command()
                   .arg("cmd /c dir a")
                   .std_out(std::move(process::Stdio::pipe()))
                   .std_err(std::move(process::Stdio::pipe()));
    auto child = cmd.spawn();
    auto result = child.wait();
    std::cout << "success: " << std::boolalpha << result.success() << '\n';

    // while (!child.try_wait().has_value()) {
    //   std::cout << child.read_stdout();
    // }
  }
}
