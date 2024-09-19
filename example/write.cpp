#include "../src/process.hpp"
#include <cassert>
#include <iomanip>
#include <iostream>

using namespace process;
int main(int argc, char *argv[]) {
  if (argc == 2 && std::string(argv[1]) == "in") { // input mode
    std::string str;
    for (int i = 0; i < 3; i += 1) {
      getline(std::cin, str);
      std::cout << "get: " << std::quoted(str) << '\n';
    }
    return 0;
  }

#ifndef _WIN32
  std::string app = std::string(argv[0]);
  std::vector<std::string> args{"in"}; // TODO:
#else
  std::string app = "cmd";
  std::vector<std::string> args{"/c", std::string(argv[0]), "in"};
#endif

  try {
    {
      auto child = Command(app).args(args).std_in(Stdio::pipe()).spawn();
      assert(child.io_stdin);
      std::vector<std::string> strs{"hello\n", "from\n", "parent\n"};
      for (auto &str : strs) {
        child.io_stdin->write(std::as_bytes(std::span{str}));
      }

      ExitStatus status = child.wait();
      std::cout << *status.code() << '\n';
    }
  } catch (const std::exception &e) {
    std::cerr << e.what();
  }
}
