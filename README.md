# Process cpp

A c++ implementation similar to Rust's [std::process](https://doc.rust-lang.org/std/process/index.html).

## Quick Start

Following quick start is for linux and mac

1. get the library to your directory
```sh
wget https://github.com/WesleyCh3n/process-cpp/raw/refs/heads/main/src/process.hpp
wget https://github.com/WesleyCh3n/process-cpp/raw/refs/heads/main/src/unix.cpp
```

2. create a new file named `example.cpp` with following:

```cpp
#include "process.hpp"

#include <iomanip>
#include <iostream>

int main(int argc, char *argv[]) {
  auto output =
      process::Command("sh").arg("-c").arg("echo hello world").output();
  std::cout << std::boolalpha << output.status.success() << '\n';
  std::cout << std::quoted(output.std_out) << '\n';
  std::cout << std::quoted(output.std_err) << '\n';
}
```

3. Compile

```sh
c++ -std=c++20 -o example example.cpp unix.cpp
```

4. `./example`

```sh
true
"hello world
"
""
```

## Usage

For more detail, check [examples](https://github.com/WesleyCh3n/std-process/tree/main/example) and [process.hpp](https://github.com/WesleyCh3n/std-process/blob/main/src/process.hpp).

```cpp
#include "process.hpp"

#include <iomanip>
#include <iostream>

int main() {
    {
        // windows. wait for output
        Output output = Command("cmd").args({"/c", "echo hello world"}).output();
        std::cout << std::quoted(output.std_out) << '\n';
        std::cout << std::quoted(output.std_err) << '\n';
    }
    {
        // unix. get child handle and wait for output
        Child child = Command("sh")
            .arg("-c")
            .arg("echo hello world")
            .std_out(Stdio::pipe())
            .std_err(Stdio::pipe())
            .spawn();
        Output output = child.wait_with_output();

        std::cout << std::quoted(output.std_out) << '\n';
        std::cout << std::quoted(output.std_err) << '\n';
    }
    {
        // write to process stdin
        std::string app = "some_app_need_stdin";
        auto child = Command(app).std_in(Stdio::pipe()).spawn();

        std::vector<std::string> strs{"hello\n", "from\n", "parent\n"};
        for (auto &str : strs) {
            child.io_stdin->write(std::as_bytes(std::span{str}));
        }
        ExitStatus status = child.wait();
    }
    {
        // pipe process one stdout to process 2 stdin
        // p1.stdout -> p2.stdin
        auto child1 = Command(app1).args(args1).std_out(Stdio::pipe()).spawn();
        auto child2 = Command(app2)
            .args(args2)
            .std_in(Stdio::from(std::move(*child1.io_stdout)))
            .spawn();
        ExitStatus status = child2.wait();
    }
}
```

## Build and Install

```sh
cmake -Bbuild .
cmake --build build -j
```

For more CMake options, see [CMakeLists.txt](https://github.com/WesleyCh3n/std-process/blob/main/CMakeLists.txt)


## Run Test

After test built,

```sh
ctest --output-on-failure --test-dir ./build/test/
```

## License
