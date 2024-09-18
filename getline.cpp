#include <iomanip>
#include <iostream>
#include <string>
using namespace std;
int main(int argc, char *argv[]) {
  string str;
  for (int i = 0; i < 3; i += 1) {
    getline(cin, str);
    cout << "get: " << std::quoted(str) << '\n';
  }
}
