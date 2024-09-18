#include <bitset>
#include <iostream>

using std::cerr;
using std::cin;
using std::cout;
int main(int argc, char *argv[]) {
  int value = strtol(argv[1], nullptr, 2);
  if (value <= 0 or value > 7) {
    cout << "only three bit can be set.\n"
            R"(ex. stdin: "001", stdout: "010", stderr: "100")";
    return 1;
  }
  auto mode = std::bitset<4>(static_cast<char>(value));
  if (mode[0]) { // stdin

  } else if (mode[1] and not mode[2]) { // stdout
    if (argc < 3) {
      cout << "missing output string\n";
      return 1;
    }
    cout << argv[2];
  } else if (mode[2] and not mode[1]) { // stderr
    if (argc < 3) {
      cout << "missing output string\n";
      return 1;
    }
    cerr << argv[2];
  } else if (mode[1] and mode[2]) {
    if (argc < 4) {
      cout << "missing output/error string\n";
      return 1;
    }
    cout << argv[2];
    cerr << argv[3];
  } else {
  }
}
