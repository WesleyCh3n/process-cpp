#ifdef _WIN32
#include <windows.h>

#include <filesystem>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <vector>

#include "process.hpp"

namespace process {
using std::atomic_bool;
using std::optional;
using std::pair;
using std::string;
using std::vector;

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

/*============================================================================*/
struct Process {
  PROCESS_INFORMATION pi;
  Process(PROCESS_INFORMATION p) : pi(p) {}
  Process(Process &&other) { *this = std::move(other); }
  Process &operator=(Process &&other) {
    if (this != &other) {
      this->pi = std::move(other.pi);
    }
    return *this;
  }
  int wait() {
    DWORD exit_code;
    if (WaitForSingleObject(pi.hProcess, INFINITE) != WAIT_OBJECT_0) {
      throw std::runtime_error("Failed to wait for single object");
    }
    if (GetExitCodeProcess(pi.hProcess, &exit_code) == 0) {
      throw std::runtime_error("Failed to get exit code process");
    }
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return exit_code;
  }
};

/*============================================================================*/
struct ExitStatus::Impl {
  optional<int> code;
  Impl() : code(std::nullopt) {}
  Impl(int result) : code(result) {}
};
ExitStatus::ExitStatus() : impl_(std::make_unique<Impl>()) {}
ExitStatus::~ExitStatus() = default;
ExitStatus::ExitStatus(ExitStatus &&other) { *this = std::move(other); }
ExitStatus &ExitStatus::operator=(ExitStatus &&other) {
  if (this != &other) {
    this->impl_ = std::move(other.impl_);
  }
  return *this;
}
bool ExitStatus::success() { return impl_->code == 0; }
optional<int> ExitStatus::code() { return impl_->code; }

/*============================================================================*/
Stdio::Stdio(Value v) : impl_(std::make_unique<Impl>(v)) {}
Stdio::~Stdio() = default;
Stdio::Stdio(Stdio &&other) { *this = std::move(other); };
Stdio &Stdio::operator=(Stdio &&other) {
  if (&other != this)
    this->impl_ = std::move(other.impl_);

  return *this;
}

struct Stdio::Impl {
  Value value;
  Impl(Value v) : value(v) {}
  pair<HANDLE, HANDLE> to_handles(uint8_t id) { //{0: in, 1: out, 2: err}
    switch (value) {
    case Value::Inherit: {
      if (id == 0)
        return {nullptr, GetStdHandle(STD_INPUT_HANDLE)};
      else if (id == 1)
        return {nullptr, GetStdHandle(STD_OUTPUT_HANDLE)};
      else if (id == 2)
        return {nullptr, GetStdHandle(STD_ERROR_HANDLE)};
      else
        throw std::runtime_error("invaled handle id");
    }
    case Value::NewPipe: {
      HANDLE handle[2];
      SECURITY_ATTRIBUTES sa = {sizeof(SECURITY_ATTRIBUTES), NULL, TRUE};
      if (!CreatePipe(&handle[0], &handle[1], &sa, 0) ||
          !SetHandleInformation(handle[0], HANDLE_FLAG_INHERIT, 0))
        throw std::runtime_error("Failed to create pipe");
      return (id == 0) ? std::make_pair(handle[1], handle[0])
                       : std::make_pair(handle[0], handle[1]);
    }
    case Value::FromPipe: {
      throw std::logic_error("unimplemented");
      return {nullptr, nullptr};
    }
    case Value::Null: {
      throw std::logic_error("unimplemented");
      return {nullptr, nullptr};
    }
    default: {
      throw std::logic_error("invalid stdio type");
      return {nullptr, nullptr};
    }
    }
  }
};

Stdio Stdio::pipe() { return Stdio(Value::NewPipe); }
Stdio Stdio::inherit() { return Stdio(Value::Inherit); }
Stdio Stdio::null() { return Stdio(Value::Null); }

/*============================================================================*/
DWORD read_handle(HANDLE handle, char buffer[], size_t size) {
  DWORD bytes_read;
  if (ReadFile(handle, buffer, size - 1, &bytes_read, NULL) > 0) {
    buffer[bytes_read] = '\0';
    return bytes_read;
  }
  return 0;
}

ChildStdin::ChildStdin() : impl_(nullptr) {}
ChildStdin::~ChildStdin() {}
ChildStdin::ChildStdin(ChildStdin &&other) { *this = std::move(other); }
ChildStdin &ChildStdin::operator=(ChildStdin &&other) {
  if (this != &other) {
    this->impl_ = std::move(other.impl_);
  }
  return *this;
}

struct ChildStdin::Impl {
  HANDLE handle;
  Impl(HANDLE h) : handle(h) {}
  ssize_t write(char buffer[], size_t size) {
    throw std::logic_error("unimplemented");
    return read_handle(this->handle, buffer, size);
  }
};

ssize_t ChildStdin::write(char buffer[], size_t size) {
  return impl_->write(buffer, size);
}

ChildStdout::ChildStdout() : impl_(nullptr) {}
ChildStdout::~ChildStdout() {}
ChildStdout::ChildStdout(ChildStdout &&other) { *this = std::move(other); }
ChildStdout &ChildStdout::operator=(ChildStdout &&other) {
  if (this != &other) {
    this->impl_ = std::move(other.impl_);
  }
  return *this;
}

struct ChildStdout::Impl {
  HANDLE handle;
  Impl(HANDLE h) : handle(h) {}
  ssize_t read(char buffer[], size_t size) {
    return read_handle(this->handle, buffer, size);
  }
};
ssize_t ChildStdout::read(char buffer[], size_t size) {
  return impl_->read(buffer, size);
}

ChildStderr::ChildStderr() : impl_(nullptr) {}
ChildStderr::~ChildStderr() {}
ChildStderr::ChildStderr(ChildStderr &&other) { *this = std::move(other); }
ChildStderr &ChildStderr::operator=(ChildStderr &&other) {
  if (this != &other) {
    this->impl_ = std::move(other.impl_);
  }
  return *this;
}

struct ChildStderr::Impl {
  HANDLE handle;
  Impl(HANDLE h) : handle(h) {}
  ssize_t read(char buffer[], size_t size) {
    return read_handle(this->handle, buffer, size);
  }
};
ssize_t ChildStderr::read(char buffer[], size_t size) {
  return impl_->read(buffer, size);
}

/*============================================================================*/
Child::Child() : impl_(nullptr){};
Child::~Child(){};
Child::Child(Child &&other) { *this = std::move(other); };
Child &Child::operator=(Child &&other) {
  if (&other != this) {
    this->impl_ = std::move(other.impl_);
  }
  return *this;
}

struct Child::Impl {
  Process pi;

  Impl(Process pi) : pi(std::move(pi)) {}

  int id() { return pi.pi.dwProcessId; }
  ExitStatus wait() {
    int code = pi.wait();
    ExitStatus status;
    status.impl_->code = code;
    return status;
  }
};
int Child::id() { return impl_->id(); }
ExitStatus Child::wait() { return impl_->wait(); }
Output Child::wait_with_output() {
  Output output;
  char stdout_buf[2048], stderr_buf[2048];
  ssize_t stdout_size = 0, stderr_size = 0;
  while (((stdout_size = this->io_stdout->read(stdout_buf, 2048)) > 0) ||
         ((stderr_size = this->io_stderr->read(stderr_buf, 2048)) > 0)) {
    if (stdout_size > 0)
      output.std_out += stdout_buf;
    if (stderr_size > 0)
      output.std_err += stderr_buf;
  }
  output.status = this->wait();
  return output;
}

/*============================================================================*/
class Command::Impl {
  string app;
  vector<string> args;
  Stdio io_stdin;
  Stdio io_stdout;
  Stdio io_stderr;

public:
  Impl()
      : app(string()), args(vector<string>()), io_stdin(Stdio::inherit()),
        io_stdout(Stdio::inherit()), io_stderr(Stdio::inherit()) {
    char path[MAX_PATH];
    UINT size = 0;
    if ((size = GetSystemDirectory(path, MAX_PATH)) == 0)
      throw std::runtime_error("failed to get system directory");
    app = (std::filesystem::path(string(path, size)) / "cmd.exe").string();
  }
  ~Impl() = default;
  void set_app(string str) { app = str; }
  void add_args(const string &arg) { args.push_back(arg); }
  void set_stdin(Stdio io) { io_stdin = std::move(io); }
  void set_stdout(Stdio io) { io_stdout = std::move(io); }
  void set_stderr(Stdio io) { io_stderr = std::move(io); }

  Child spawn() {
    string arg;
    if (!args.empty()) {
      arg += args[0];
      for (int i = 1; i < args.size(); i += 1) {
        arg += " " + args[i];
      }
    }
    auto [our_stdin, their_stdin] = io_stdin.impl_->to_handles(0);
    auto [our_stdout, their_stdout] = io_stdout.impl_->to_handles(1);
    auto [our_stderr, their_stderr] = io_stderr.impl_->to_handles(2);
    PROCESS_INFORMATION pi = {0};
    STARTUPINFO si = {0};
    si.cb = sizeof(STARTUPINFO);
    si.hStdInput = their_stdin;
    si.hStdOutput = their_stdout;
    si.hStdError = their_stderr;
    si.dwFlags |= STARTF_USESTDHANDLES;
    if (!CreateProcess(app.c_str(), const_cast<char *>(arg.c_str()), nullptr,
                       nullptr, TRUE, 0, nullptr, nullptr, &si, &pi)) {
      throw std::runtime_error("Failed to create process");
    }
    if (their_stdin != GetStdHandle(STD_INPUT_HANDLE)) {
      CloseHandle(their_stdin);
    }
    if (their_stderr != GetStdHandle(STD_ERROR_HANDLE)) {
      CloseHandle(their_stderr);
    }
    if (their_stdout != GetStdHandle(STD_OUTPUT_HANDLE)) {
      CloseHandle(their_stdout);
    }
    Child s;
    if (our_stdin != nullptr) {
      s.io_stdin = std::make_optional<ChildStdin>();
      s.io_stdin->impl_ = std::make_unique<ChildStdin::Impl>(our_stdin);
    }
    if (our_stdout != nullptr) {
      s.io_stdout = std::make_optional<ChildStdout>();
      s.io_stdout->impl_ = std::make_unique<ChildStdout::Impl>(our_stdout);
    }
    if (our_stderr != nullptr) {
      s.io_stderr = std::make_optional<ChildStderr>();
      s.io_stderr->impl_ = std::make_unique<ChildStderr::Impl>(our_stderr);
    }

    s.impl_ = std::make_unique<Child::Impl>(Process(pi));
    return s;
  }
};

Command::Command(const string &app) : impl_(std::make_unique<Impl>()) {
  if (!app.empty())
    impl_->set_app(app);
};
Command::~Command(){};
Command::Command(Command &&other) { *this = std::move(other); }
Command &Command::operator=(Command &&other) {
  if (this != &other) {
    this->impl_ = std::move(other.impl_);
  }
  return *this;
}

Command &&Command::arg(const string &arg) {
  impl_->add_args(arg);
  return std::move(*this);
}
Command &&Command::args(const vector<string> &args) {
  for (const string &arg : args)
    impl_->add_args(arg);
  return std::move(*this);
}
Command &&Command::std_out(Stdio io) {
  impl_->set_stdout(std::move(io));
  return std::move(*this);
}
Command &&Command::std_err(Stdio io) {
  impl_->set_stderr(std::move(io));
  return std::move(*this);
}
Child Command::spawn() { return impl_->spawn(); }
Output Command::output() {
  impl_->set_stdout(Stdio::pipe());
  impl_->set_stderr(Stdio::pipe());
  Child child = impl_->spawn();
  return child.wait_with_output();
}
} // namespace process

#endif
