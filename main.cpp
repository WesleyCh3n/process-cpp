#include "src/process.hpp"

#include <windows.h>

#include <iomanip>
#include <iostream>
#include <thread>
#include <vector>

using namespace std;

inline std::string GetLastErrorAsString() {
  DWORD errorMessageID = ::GetLastError();
  if (errorMessageID == 0) {
    return std::string(); // No error message has been recorded
  }

  LPSTR messageBuffer = nullptr;
  size_t size = FormatMessageA(
      FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
          FORMAT_MESSAGE_IGNORE_INSERTS,
      NULL, errorMessageID, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
      (LPSTR)&messageBuffer, 0, NULL);
  std::string message(messageBuffer, size);
  LocalFree(messageBuffer);
  return message;
}

void pipe_relay(HANDLE source, HANDLE destination) {
  std::vector<char> buffer(4096);
  DWORD bytes_read = 0;
  DWORD bytes_written = 0;

  while (true) {
    // Read from source pipe
    if (!ReadFile(source, buffer.data(), buffer.size(), &bytes_read, nullptr)) {
      break; // Exit on error or EOF
    }

    if (bytes_read == 0) {
      break; // EOF, exit the loop
    }

    DWORD total_written = 0;
    while (total_written < bytes_read) {
      if (!WriteFile(destination, buffer.data() + total_written,
                     bytes_read - total_written, &bytes_written, nullptr)) {
        break; // Exit on error
      }
      total_written += bytes_written;
    }
  }
  std::cout << "break\n";
}

void test() {
  HANDLE out_read, out_write; // Pipe handles for reading and writing
  SECURITY_ATTRIBUTES sa = {sizeof(SECURITY_ATTRIBUTES), NULL, TRUE};

  // Create the pipe
  if (!CreatePipe(&out_read, &out_write, &sa, 0) ||
      !SetHandleInformation(out_read, HANDLE_FLAG_INHERIT, 0)) {
    throw std::runtime_error("Failed to create pipe");
  }

  // First process (hello.exe)
  PROCESS_INFORMATION pi_hello = {0};
  STARTUPINFO si_hello = {0};
  si_hello.cb = sizeof(STARTUPINFO);
  si_hello.hStdInput = nullptr;    // No input for the first process
  si_hello.hStdOutput = out_write; // Write to the pipe
  si_hello.hStdError = out_write;
  si_hello.dwFlags |= STARTF_USESTDHANDLES;

  if (!CreateProcess(nullptr, const_cast<char *>(R"(.\build\Debug\hello.exe)"),
                     nullptr, nullptr, TRUE, 0, nullptr, nullptr, &si_hello,
                     &pi_hello)) {
    std::cerr << "Failed to create first process" << std::endl;
    return;
  }

  // Close the write handle for the first process as it's inherited
  CloseHandle(out_write);

  HANDLE dup_relay_read;
  if (!DuplicateHandle(GetCurrentProcess(), out_read, GetCurrentProcess(),
                       &dup_relay_read, 0, TRUE, DUPLICATE_SAME_ACCESS)) {
    throw std::runtime_error("Failed to duplicate relay handle");
  }

  // Create a new set of pipes for the relay
  HANDLE relay_read, relay_write;
  if (!CreatePipe(&relay_read, &relay_write, &sa, 0)) {
    throw std::runtime_error("Failed to create relay pipe");
  }

  // Spawn a thread to relay data from `out_read` to `relay_write`
  std::thread relay_thread(pipe_relay, dup_relay_read, relay_write);
  relay_thread.detach();

  // Second process (test.exe)
  PROCESS_INFORMATION pi_test = {0};
  STARTUPINFO si_test = {0};
  si_test.cb = sizeof(STARTUPINFO);
  si_test.hStdInput = relay_read; // Read from the relay pipe
  si_test.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE); // Write to the console
  si_test.hStdError = GetStdHandle(STD_ERROR_HANDLE);
  si_test.dwFlags |= STARTF_USESTDHANDLES;

  if (!CreateProcess(nullptr, const_cast<char *>(R"(.\build\Debug\test.exe)"),
                     nullptr, nullptr, TRUE, 0, nullptr, nullptr, &si_test,
                     &pi_test)) {
    std::cerr << "Failed to create second process" << std::endl;
    return;
  }

  // Close the read handle for the relay pipe
  CloseHandle(relay_read);

  // Wait for both processes to finish
  std::cout << "wait1\n";
  WaitForSingleObject(pi_hello.hProcess, INFINITE);
  std::cout << "wait2\n";
  WaitForSingleObject(pi_test.hProcess, INFINITE);

  // Clean up
  // relay_thread.join();
  CloseHandle(out_write);
  CloseHandle(relay_write);

  CloseHandle(pi_hello.hProcess);
  CloseHandle(pi_hello.hThread);

  CloseHandle(pi_test.hProcess);
  CloseHandle(pi_test.hThread);
}

int main(int argc, char *argv[]) {
  try {
    // test();
    using namespace process;
    {
      auto child = Command(R"(.\build\Debug\hello.exe)")
                       // .arg("/c")
                       .std_out(Stdio::pipe())
                       .spawn();
      // char buffer[2048];
      // ssize_t size = 0;
      if (child.io_stdout.has_value()) {
        auto output = Command(R"(.\build\Debug\test.exe)")
                          .std_in(Stdio::from(std::move(*child.io_stdout)))
                          .spawn();
        output.wait();
        // cout << *output.status.code() << '\n';
        // cout << "out: " << output.std_out << '\n';
        // cout << "err: " << output.std_err << '\n';
        // auto child2 = Command(R"(.\build\Debug\test.exe)")
        //                   // .arg("h")
        //                   .std_in(Stdio::from(std::move(*child.io_stdout)))
        //                   .spawn();
        // child2.wait();
      }
      child.wait();
    }
    /* std::cout << std::string(80, '=') << std::endl;
    {
      auto child = process::Command()
                       .arg(argv[1])
                       .std_out(process::Stdio::pipe())
                       .std_err(process::Stdio::pipe())
                       .spawn();
      std::cout << "pid: " << child.id() << '\n';
      // std::this_thread::sleep_for(std::chrono::seconds(3));
      char buffer[2048];
      ssize_t size = 0;
      if (child.io_stdout.has_value())
        while ((size = child.io_stdout->read(buffer, 2048)) > 0) {
          std::cout << "[OUT]: " << std::quoted(buffer) << '\n';
        }
      if (child.io_stderr.has_value()) {
        while ((size = child.io_stderr->read(buffer, 2048)) > 0) {
          std::cout << "[ERR]: " << std::quoted(buffer) << '\n';
        }
      }
      child.wait();
    }
    std::cout << std::string(80, '=') << std::endl; */
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
  }
}
