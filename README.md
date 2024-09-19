# Process cpp

A c++ implementation similar to Rust's [std::process](https://doc.rust-lang.org/std/process/index.html).

## Build

```sh
cmake -Bbuild .
cmake --build build -j
```

For more cmake options, see [CMakeLists.txt](https://github.com/WesleyCh3n/std-process/blob/main/CMakeLists.txt)

Tested environments:
- Windows 11 22H2
- Ubuntu
- MacOSX

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

## Run Test

After test built,

```sh
ctest --test-dir ./build/test/
```

## License
