#include "../src/process.hpp"

#include <cassert>
#include <iomanip>
#include <iostream>

using namespace process;
int main(int argc, char *argv[]) {
  if (argc == 2 && std::string(argv[1]) == "out") { // output mode
    std::vector<std::string> strs{"hello\n", "from\n", "process 1\n"};
    for (auto &str : strs)
      std::cout << str;
    return 0;
  }
  if (argc == 2 && std::string(argv[1]) == "in") { // input mode
    std::string str;
    for (int i = 0; i < 3; i += 1) {
      getline(std::cin, str);
      std::cout << "get: " << std::quoted(str) << '\n';
    }
    return 0;
  }

  try {
    {
#ifndef _WIN32
      std::string app = std::string(argv[0]);
      std::vector<std::string> args1{"out"};
      std::vector<std::string> args2{"in"};
#else
      std::string app = "cmd";
      std::vector<std::string> args1{"/c", std::string(argv[0]), "out"};
      std::vector<std::string> args2{"/c", std::string(argv[0]), "in"};
#endif
      auto child1 = Command(app).args(args1).std_out(Stdio::pipe()).spawn();
      assert(child1.io_stdout);
      auto child2 = Command(app)
                        .args(args2)
                        .std_in(Stdio::from(std::move(*child1.io_stdout)))
                        .spawn();

      ExitStatus status = child2.wait();
    }
  } catch (const std::exception &e) {
    std::cerr << e.what();
  }
}
