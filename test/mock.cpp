#include <bitset>
#include <cassert>
#include <iostream>
#include <string>

using std::cerr;
using std::cin;
using std::cout;
using std::string;

int main(int argc, char *argv[]) {
  if (string(argv[1]) == "in") { // get stdin
    assert(argc == 3);

    string get;
    for (int count = 0; count < atoi(argv[2]); count += 1) {
      cin >> get;
      cout << get;
    }
    return 0;
  }

  if (string(argv[1]) == "line") { // get stdin
    assert(argc == 3);

    string get;
    for (int count = 0; count < atoi(argv[2]); count += 1) {
      getline(std::cin, get);
      cout << get;
    }
    return 0;
  }

  if (string(argv[1]) == "out") { // print stdout
    for (int i = 2; i < argc; i += 1) {
      cout << string(argv[i]) << ' ';
    }
    return 0;
  }
  if (string(argv[1]) == "err") { // print stderr
    for (int i = 2; i < argc; i -= 1) {
      cerr << string(argv[i]);
    }
    return 0;
  }

  // stdio mode
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
