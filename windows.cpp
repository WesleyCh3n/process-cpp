#ifdef _WIN32
#include <atomic>
#include <iomanip>
#include <windows.h>

#include <iostream>
#include <optional>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>

#include "queue.hpp"
#include "util.hpp"

namespace process {
using std::atomic_bool;
using std::optional;
using std::pair;
using std::string;
using std::thread;
using std::vector;

inline std::string GetLastErrorAsString() {
  // Get the error message ID, if any.
  DWORD errorMessageID = ::GetLastError();
  if (errorMessageID == 0) {
    return std::string(); // No error message has been recorded
  }

  LPSTR messageBuffer = nullptr;

  // Ask Win32 to give us the string version of that message ID.
  // The parameters we pass in, tell Win32 to create the buffer that holds the
  // message for us (because we don't yet know how long the message string will
  // be).
  size_t size = FormatMessageA(
      FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
          FORMAT_MESSAGE_IGNORE_INSERTS,
      NULL, errorMessageID, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
      (LPSTR)&messageBuffer, 0, NULL);

  // Copy the error message into a std::string.
  std::string message(messageBuffer, size);

  // Free the Win32's string's buffer.
  LocalFree(messageBuffer);

  return message;
}

class Stdio {
public:
  enum Type { INHERIT, PIPE };

  Stdio(Type t) : type_(t) {}
  Type type() const { return type_; }
  Stdio(const Stdio &) = delete;
  Stdio &operator=(const Stdio &) = delete;

  Stdio(Stdio &&other) { *this = std::move(other); };
  Stdio &operator=(Stdio &&other) {
    if (&other != this) {
      this->read_ = other.read_;
      this->write_ = other.write_;
      this->type_ = other.type_;
      other.read_ = nullptr;
      other.write_ = nullptr;
    }
    return *this;
  }

  static Stdio pipe() { return make_pipe(); }
  static Stdio inherit() { return Stdio(Type::INHERIT); }

  friend class Child;
  friend class Command;

private:
  Type type_;
  HANDLE read_ = nullptr;
  HANDLE write_ = nullptr;
  void close_write() {
    CloseHandle(write_);
    write_ = nullptr;
  };
  void close_read() {
    CloseHandle(read_);
    read_ = nullptr;
  };
  static Stdio make_pipe() {
    Stdio io = Stdio(Type::PIPE);
    SECURITY_ATTRIBUTES sa = {sizeof(SECURITY_ATTRIBUTES), NULL, TRUE};
    if (!CreatePipe(&io.read_, &io.write_, &sa, 0) ||
        !SetHandleInformation(io.read_, HANDLE_FLAG_INHERIT, 0)) {
      throw std::runtime_error("Failed to create pipe");
    }
    return io;
  }
};

class ExitStatus {
  DWORD code_;
  friend class Child;

public:
  bool success() { return code_ == 0; }
  int32_t code() { return code_; }
};

class Child {
  std::unique_ptr<PROCESS_INFORMATION> pi_;
  Stdio std_out_io_, std_err_io_;
  atomic_bool finish_;
  ThreadSafeQueue<char> out_queue_;
  ThreadSafeQueue<char> err_queue_;
  vector<thread> io_thread_;

  void read_pipe(HANDLE pipe_handle, ThreadSafeQueue<char> &q, bool out) {
    CHAR buffer[4096];
    DWORD bytesRead;
    while (
        ReadFile(pipe_handle, buffer, sizeof(buffer) - 1, &bytesRead, NULL)) {
      if (bytesRead == 0) {
        break;
      }
      buffer[bytesRead] = '\0';
      std::cout << (out ? "OUT" : "ERR") << " buffer: " << std::quoted(buffer);
      // q.push_bulk(vector<char>(buffer, buffer + bytesRead));
    }
  }

public:
  Child(std::unique_ptr<PROCESS_INFORMATION> pi, Stdio std_out_io,
        Stdio std_err_io)
      : pi_(std::move(pi)), std_out_io_(std::move(std_out_io)),
        std_err_io_(std::move(std_err_io)) {
    std::cout << "start\n";
    if (std_out_io_.type() == Stdio::Type::PIPE) {
      io_thread_.emplace_back(std::thread([&]() {
        CHAR buffer[4096];
        DWORD bytesRead;
        while (true) {
          BOOL success = ReadFile(std_out_io_.read_, buffer, sizeof(buffer) - 1,
                                  &bytesRead, NULL);
          if (!success || bytesRead == 0) {
            break; // Exit the loop if no more data
          }
          buffer[bytesRead] = '\0'; // Null-terminate the string
          std::cout << "get out: " << buffer;
        }
      }));
      // io_thread_.back().detach();
    }
    if (std_err_io_.type() == Stdio::Type::PIPE) {
      io_thread_.emplace_back(std::thread([&]() {
        CHAR buffer[4096];
        DWORD bytesRead;
        while (true) {
          BOOL success = ReadFile(std_err_io_.read_, buffer, sizeof(buffer) - 1,
                                  &bytesRead, NULL);
          if (!success || bytesRead == 0) {
            break; // Exit the loop if no more data
          }
          buffer[bytesRead] = '\0'; // Null-terminate the string
          std::cout << "get err: " << buffer;
        }
      }));
      // io_thread_.back().detach();
    }
  }
  ~Child() {}

  ExitStatus wait() {
    ExitStatus status;
    if (WaitForSingleObject(pi_->hProcess, INFINITE) != WAIT_OBJECT_0) {
      std::cout << GetLastErrorAsString() << '\n';
      throw std::runtime_error("Failed to wait for single object");
    }
    // if (GetExitCodeProcess(pi_->hProcess, &status.code_) ==
    //     0) { // TODO: error handle
    //   std::cout << GetLastErrorAsString() << '\n';
    //   throw std::runtime_error("Failed to get exit code process");
    // }
    std_out_io_.close_read();
    std_err_io_.close_read();
    for (auto &t : io_thread_) {
      t.join();
    }
    return status;
  }

  optional<ExitStatus> try_wait() {
    if (WaitForSingleObject(pi_->hProcess, 0) == WAIT_OBJECT_0) {
      ExitStatus status;
      if (GetExitCodeProcess(pi_->hProcess, &status.code_) ==
          0) { // TODO: error handle
        std::cout << GetLastErrorAsString() << '\n';
      }
      return status;
    }
    return std::nullopt;
  }

  void kill() { throw std::logic_error("Function not yet implemented"); }

  string read_stdout(char delimiter = '\n') {
    string result;
    while (!out_queue_.empty()) {
      result.push_back(' ');
      out_queue_.pop(result.back());
      if (result.back() == delimiter) {
        break;
      }
    }
    return result;
  }
};

// class Output {
//   int exist_status;
//   vector<char> std_out;
//   vector<char> std_err;
// };

class Command {
  string app_;
  string arg_;
  Stdio std_out_io_, std_err_io_; // default to inherit

public:
  Command()
      : app_(string()), std_out_io_(std::move(Stdio::inherit())),
        std_err_io_(std::move(Stdio::inherit())) {}
  Command &&arg(string arg) {
    arg_ = arg;
    return std::move(*this);
  }
  Command &&std_out(Stdio io) {
    std_out_io_ = std::move(io);
    return std::move(*this);
  };
  Command &&std_err(Stdio io) {
    std_err_io_ = std::move(io);
    return std::move(*this);
  };
  Child spawn() {
    auto pi = std::make_unique<PROCESS_INFORMATION>();
    STARTUPINFO si = {0};
    si.cb = sizeof(STARTUPINFO);
    si.hStdOutput = std_out_io_.type() == Stdio::Type::PIPE
                        ? std_out_io_.write_
                        : GetStdHandle(STD_OUTPUT_HANDLE);
    si.hStdError = std_err_io_.type() == Stdio::Type::PIPE
                       ? std_err_io_.write_
                       : GetStdHandle(STD_ERROR_HANDLE);
    si.dwFlags |= STARTF_USESTDHANDLES;
    // Create the child process
    if (!CreateProcess(nullptr, const_cast<char *>(arg_.c_str()), nullptr,
                       nullptr, TRUE, 0, nullptr, nullptr, &si, pi.get())) {
      throw std::runtime_error("Failed to create process");
    }

    if (std_out_io_.type() == Stdio::Type::PIPE)
      std_out_io_.close_write();
    if (std_err_io_.type() == Stdio::Type::PIPE)
      std_err_io_.close_write();

    return Child(std::move(pi), std::move(std_out_io_), std::move(std_err_io_));
  }
  // eager run
  // void status() {}
  // void output() {}
};
} // namespace process

#endif
