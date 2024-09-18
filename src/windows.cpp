#ifdef _WIN32
#include <windows.h>

#include <filesystem>
#include <format>
#include <iostream>
#include <optional>
#include <span>
#include <stdexcept>
#include <thread>
#include <vector>

#include "process.hpp"

namespace process {
using std::optional;
using std::pair;
using std::span;
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

DWORD read_handle(HANDLE handle, span<std::byte> buffer) {
  DWORD bytes_read;
  if (ReadFile(handle, buffer.data(), static_cast<DWORD>(buffer.size()),
               &bytes_read, NULL) > 0) {
    // buffer[bytes_read] = '\0';
    return bytes_read;
  }
  return 0;
}

size_t write_handle(HANDLE handle, span<const std::byte> buffer) {
  DWORD written;
  if (!WriteFile(handle, buffer.data(), static_cast<DWORD>(buffer.size()),
                 &written, nullptr)) {
    throw std::runtime_error("failed to write file to handle");
  }
  return static_cast<size_t>(written);
}

static pair<HANDLE, HANDLE> spawn_pipe_relay(HANDLE source, bool our_readable,
                                             bool their_handle_inheritable) {
  HANDLE dup_source;
  if (!DuplicateHandle(GetCurrentProcess(), source, GetCurrentProcess(),
                       &dup_source, 0, TRUE, DUPLICATE_SAME_ACCESS)) {
    throw std::runtime_error("failed to duplicate relay handle");
  }
  HANDLE reader, writer; // read, write
  SECURITY_ATTRIBUTES sa = {sizeof(SECURITY_ATTRIBUTES), NULL, TRUE};
  if (!CreatePipe(&reader, &writer, &sa, 0))
    throw std::runtime_error("Failed to create pipe");

  std::thread relay_thread(
      [](HANDLE source, HANDLE destination) {
        char buffer[4096];
        DWORD bytes_read = 0;
        DWORD bytes_written = 0;

        while (true) {
          // Read from source pipe
          if (!ReadFile(source, buffer, 4096, &bytes_read, nullptr)) {
            break; // Exit on error or EOF
          }

          if (bytes_read == 0) {
            break; // EOF, exit the loop
          }

          DWORD total_written = 0;
          while (total_written < bytes_read) {
            if (!WriteFile(destination, buffer + total_written,
                           bytes_read - total_written, &bytes_written,
                           nullptr)) {
              break; // Exit on error
            }
            total_written += bytes_written;
          }
        }
        CloseHandle(destination);
        CloseHandle(source);
      },
      dup_source, writer);
  relay_thread.detach();
  return std::make_pair(nullptr, reader);
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
struct ChildStdin::Impl {
  HANDLE handle;
  Impl(HANDLE h) : handle(h) {}
  size_t write(span<const std::byte> buffer) {
    return write_handle(this->handle, buffer);
  }
};

ChildStdin::ChildStdin() : impl_(nullptr) {}
ChildStdin::~ChildStdin() {}
ChildStdin::ChildStdin(ChildStdin &&other) { *this = std::move(other); }
ChildStdin &ChildStdin::operator=(ChildStdin &&other) {
  if (this != &other) {
    this->impl_ = std::move(other.impl_);
  }
  return *this;
}

size_t ChildStdin::write(span<const std::byte> buffer) {
  return impl_->write(buffer);
}

struct ChildStdout::Impl {
  HANDLE handle;
  Impl(HANDLE h) : handle(h) {}
  ssize_t read(span<std::byte> buffer) {
    return read_handle(this->handle, buffer);
  }
};
ChildStdout::ChildStdout() : impl_(nullptr) {}
ChildStdout::~ChildStdout() {}
ChildStdout::ChildStdout(ChildStdout &&other) { *this = std::move(other); }
ChildStdout &ChildStdout::operator=(ChildStdout &&other) {
  if (this != &other) {
    this->impl_ = std::move(other.impl_);
  }
  return *this;
}

ssize_t ChildStdout::read(span<std::byte> buffer) {
  return impl_->read(buffer);
}

struct ChildStderr::Impl {
  HANDLE handle;
  Impl(HANDLE h) : handle(h) {}
  ssize_t read(span<std::byte> buffer) {
    return read_handle(this->handle, buffer);
  }
};
ChildStderr::ChildStderr() : impl_(nullptr) {}
ChildStderr::~ChildStderr() {}
ChildStderr::ChildStderr(ChildStderr &&other) { *this = std::move(other); }
ChildStderr &ChildStderr::operator=(ChildStderr &&other) {
  if (this != &other) {
    this->impl_ = std::move(other.impl_);
  }
  return *this;
}

ssize_t ChildStderr::read(span<std::byte> buffer) {
  return impl_->read(buffer);
}

/*============================================================================*/
struct Stdio::Impl {
  Value value;
  Impl(Value v) : value(v), other(nullptr) {}
  HANDLE other;
  //        parent, handle
  // stdin: write, read
  // stdout: read, write
  // stderr: read, write
  pair<HANDLE, HANDLE> to_handles(uint8_t id) { //{0: in, 1: out, 2: err}
    switch (value) {
    case Value::Inherit: {
      if (id == 0)
        return {GetStdHandle(STD_INPUT_HANDLE), nullptr};
      else if (id == 1)
        return {nullptr, GetStdHandle(STD_OUTPUT_HANDLE)};
      else if (id == 2)
        return {nullptr, GetStdHandle(STD_ERROR_HANDLE)};
      else
        throw std::runtime_error("invalid handle id");
    }
    case Value::NewPipe: {
      HANDLE handle[2]; // read, write
      SECURITY_ATTRIBUTES sa = {sizeof(SECURITY_ATTRIBUTES), NULL, TRUE};
      if (!CreatePipe(&handle[0], &handle[1], &sa, 0))
        throw std::runtime_error("failed to create pipe");
      if (id != 0 && !SetHandleInformation(handle[0], HANDLE_FLAG_INHERIT, 0))
        throw std::runtime_error("failed set handle information");
      return (id == 0) ? std::make_pair(handle[1], handle[0])
                       : std::make_pair(handle[0], handle[1]);
    }
    case Value::FromPipe: {
      if (id == 0)
        return spawn_pipe_relay(other, true, true);
      else if (id == 1)
        return {nullptr, nullptr};
      else if (id == 2)
        return {nullptr, nullptr};
      else
        throw std::runtime_error("invalid handle id");
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

Stdio::Stdio(Value v) : impl_(std::make_unique<Impl>(v)) {}
Stdio::~Stdio() = default;
Stdio::Stdio(Stdio &&other) { *this = std::move(other); };
Stdio &Stdio::operator=(Stdio &&other) {
  if (&other != this)
    this->impl_ = std::move(other.impl_);

  return *this;
}

Stdio Stdio::pipe() { return Stdio(Value::NewPipe); }
Stdio Stdio::inherit() { return Stdio(Value::Inherit); }
Stdio Stdio::null() { return Stdio(Value::Null); }
Stdio Stdio::from(ChildStdin other) {
  Stdio io = Stdio(Value::FromPipe);
  io.impl_->other = other.impl_->handle;
  return io;
}
Stdio Stdio::from(ChildStdout other) {
  Stdio io = Stdio(Value::FromPipe);
  io.impl_->other = other.impl_->handle;
  return io;
}
Stdio Stdio::from(ChildStderr other) {
  Stdio io = Stdio(Value::FromPipe);
  io.impl_->other = other.impl_->handle;
  return io;
}

/*============================================================================*/
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
  void kill() {
    if (not TerminateProcess(pi.pi.hProcess, 1)) {
      throw std::runtime_error(
          std::format("failed to terminate process: pid[{}]", id()));
    }
  }
};
Child::Child() : impl_(nullptr){};
Child::~Child(){};
Child::Child(Child &&other) { *this = std::move(other); };
Child &Child::operator=(Child &&other) {
  if (&other != this) {
    this->impl_ = std::move(other.impl_);
  }
  return *this;
}

int Child::id() { return impl_->id(); }
ExitStatus Child::wait() { return impl_->wait(); }
void Child::kill() { impl_->kill(); }
Output Child::wait_with_output() {
  Output output;
  std::byte stdout_buf[2048], stderr_buf[2048];
  ssize_t stdout_size = 0, stderr_size = 0;
  while (((stdout_size = this->io_stdout->read(span{stdout_buf})) > 0) ||
         ((stderr_size = this->io_stderr->read(span{stderr_buf})) > 0)) {
    if (stdout_size > 0)
      output.std_out +=
          string(reinterpret_cast<const char *>(stdout_buf), stdout_size);
    if (stderr_size > 0)
      output.std_err +=
          string(reinterpret_cast<const char *>(stderr_buf), stderr_size);
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
  optional<string> cwd;
  bool inherit_env;
  vector<pair<string, string>> envs;

public:
  Impl()
      : app(string()), args(vector<string>()), io_stdin(Stdio::inherit()),
        io_stdout(Stdio::inherit()), io_stderr(Stdio::inherit()),
        inherit_env(true) {
    char path[MAX_PATH];
    UINT size = 0;
    if ((size = GetSystemDirectory(path, MAX_PATH)) == 0)
      throw std::runtime_error("failed to get system directory");
    app = (std::filesystem::path(string(path, size)) / "cmd.exe").string();
  }
  ~Impl() = default;
  void set_app(string str) {
    std::filesystem::path name(str);
    if (name.extension() != ".exe") {
      name.replace_extension(".exe");
    }
    app = find_exe_path(name.string());
  }
  void add_args(const string &arg) { args.push_back(arg); }
  void set_stdin(Stdio io) { io_stdin = std::move(io); }
  void set_stdout(Stdio io) { io_stdout = std::move(io); }
  void set_stderr(Stdio io) { io_stderr = std::move(io); }
  void set_cwd(const string &path) {
    if (!std::filesystem::exists(path))
      throw std::runtime_error(std::format("{} not exist", path));
    cwd = path;
  }
  void clear_env() { inherit_env = false; }
  void add_env(const string &key, const string &val) {
    envs.push_back({key, val});
  }

  std::string find_exe_path(const std::string &name) {
    char path[MAX_PATH];
    if (SearchPath(nullptr, name.c_str(), NULL, MAX_PATH, path, NULL) == 0) {
      throw std::runtime_error(std::format("executable not found: {}", name));
    }
    return std::string(path);
  }
  string build_arg() {
    string arg;
    if (!args.empty()) {
      arg += args[0];
      for (int i = 1; i < args.size(); i += 1) {
        arg += " " + args[i];
      }
    }
    return arg;
  }
  string build_env() {
    if (inherit_env) { // find add and update
      for (auto &[k, v] : envs) {
        if (not SetEnvironmentVariable(k.c_str(), v.c_str())) {
          throw std::runtime_error(
              std::format("failed to set environment variable [{}: {}]", k, v));
        }
      }
      return string();
    }
    // not inherit build from scratch
    string result;
    if (!envs.empty()) {
      for (auto &[k, v] : envs) {
        result += std::format("{}={}", k, v);
        result.push_back('\0');
      }
      result.push_back('\0');
    }
    return result;
  }

  Child spawn() {
    auto [our_stdin, their_stdin] = io_stdin.impl_->to_handles(0);    // w, r
    auto [our_stdout, their_stdout] = io_stdout.impl_->to_handles(1); // r, w
    auto [our_stderr, their_stderr] = io_stderr.impl_->to_handles(2); // r, w
    PROCESS_INFORMATION pi = {0};
    STARTUPINFO si = {0};
    si.cb = sizeof(STARTUPINFO);
    si.hStdInput = their_stdin;
    si.hStdOutput = their_stdout;
    si.hStdError = their_stderr;
    si.dwFlags |= STARTF_USESTDHANDLES;
    string args = build_arg();
    string envs = build_env();
    if (!CreateProcess(
            app.c_str(),                      // Application name
            const_cast<char *>(args.c_str()), // Command line
            nullptr,                          // process handle not inheritable
            nullptr,                          // thread handle not inheritable
            TRUE, // inheritable handle in the calling process is
                  // inherited by the new process
            0,    // control the priority class and the creation of the process
            (envs.empty() ? nullptr : (LPVOID)envs.c_str()), // env string
            (cwd.has_value() ? cwd->c_str() : nullptr), // current directory
            &si,                                        //
            &pi)                                        //
    ) {
      throw std::runtime_error("Failed to create process");
    }
    if (their_stdin != GetStdHandle(STD_INPUT_HANDLE)) {
      CloseHandle(their_stdin);
    }
    if (their_stdout != GetStdHandle(STD_OUTPUT_HANDLE)) {
      CloseHandle(their_stdout);
    }
    if (their_stderr != GetStdHandle(STD_ERROR_HANDLE)) {
      CloseHandle(their_stderr);
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
Command &&Command::std_in(Stdio io) {
  impl_->set_stdin(std::move(io));
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
Command &&Command::current_dir(const std::string &path) {
  impl_->set_cwd(path);
  return std::move(*this);
}
Command &&Command::env(const string &key, const string &value) {
  impl_->add_env(key, value);
  return std::move(*this);
}
Command &&Command::env_clear() {
  impl_->clear_env();
  return std::move(*this);
}
Child Command::spawn() { return impl_->spawn(); }
ExitStatus Command::status() {
  // impl_->set_stdin(Stdio::inherit());
  impl_->set_stdout(Stdio::inherit());
  impl_->set_stderr(Stdio::inherit());
  Child child = impl_->spawn();
  return child.wait();
}

Output Command::output() {
  // impl_->set_stdin(Stdio::inherit());
  impl_->set_stdout(Stdio::pipe());
  impl_->set_stderr(Stdio::pipe());
  Child child = impl_->spawn();
  return child.wait_with_output();
}
} // namespace process

#endif
