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

using std::optional;
using std::pair;
using std::span;
using std::string;
using std::vector;

namespace Handle {
size_t read(HANDLE handle, span<std::byte> buffer) {
  DWORD bytes_read;
  if (!ReadFile(handle, buffer.data(), static_cast<DWORD>(buffer.size()),
                &bytes_read, NULL)) {
    return 0;
  }
  return static_cast<size_t>(bytes_read);
}

size_t write(HANDLE handle, span<const std::byte> buffer) {
  DWORD written;
  if (!WriteFile(handle, buffer.data(), static_cast<DWORD>(buffer.size()),
                 &written, nullptr)) {
    // FIX: throw?
    throw std::runtime_error("failed to write file to handle");
  }
  return static_cast<size_t>(written);
}

size_t read_to_end(HANDLE handle, vector<std::byte> &buffer) {
  size_t buf_init_len = size(buffer);
  vector<std::byte> tmp(2048);
  size_t tmp_size = 0;
  while ((tmp_size = read(handle, tmp)) > 0) {
    buffer.insert(end(buffer), begin(tmp), begin(tmp) + tmp_size);
  }
  return size(buffer) - buf_init_len;
}

size_t read_to_string(HANDLE handle, string &buffer) {
  size_t buf_init_len = size(buffer);
  string tmp(2048, '\0');
  size_t tmp_size = 0;
  while ((tmp_size = read(handle, std::as_writable_bytes(span{tmp}))) > 0) {
    buffer.insert(end(buffer), begin(tmp), begin(tmp) + tmp_size);
  }
  return size(buffer) - buf_init_len;
}

void read2(HANDLE h1, vector<std::byte> &buf1, HANDLE h2,
           vector<std::byte> &buf2) {}
void read2_to_string(HANDLE h1, string &buf1, HANDLE h2, string &buf2) {
  string tmp1(2048, '\0'), tmp2(2048, '\0');
  size_t size1 = 0, size2 = 0;
  while ((size1 = read(h1, std::as_writable_bytes(span{tmp1}))) > 0 ||
         (size2 = read(h2, std::as_writable_bytes(span{tmp2}))) > 0) {
    if (size1 > 0) {
      buf1.insert(end(buf1), begin(tmp1), begin(tmp1) + size1);
    }
    if (size2 > 0) {
      buf2.insert(end(buf2), begin(tmp2), begin(tmp2) + size2);
    }
  }
}
} // namespace Handle

namespace process {

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
  void kill() {
    if (not TerminateProcess(pi.hProcess, 1)) {
      throw std::runtime_error(
          std::format("failed to terminate process: pid[{}]", id()));
    }
  }
  DWORD id() { return pi.dwProcessId; }
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
};
ChildStdin::ChildStdin() : impl_(nullptr) {}
ChildStdin::~ChildStdin() {}
ChildStdin::ChildStdin(ChildStdin &&other) { *this = std::move(other); }
ChildStdin &ChildStdin::operator=(ChildStdin &&other) {
  if (this != &other)
    this->impl_ = std::move(other.impl_);
  return *this;
}
size_t ChildStdin::write(span<const std::byte> buffer) {
  return Handle::write(impl_->handle, buffer);
}

struct ChildStdout::Impl {
  HANDLE handle;
  Impl(HANDLE h) : handle(h) {}
};
ChildStdout::ChildStdout() : impl_(nullptr) {}
ChildStdout::~ChildStdout() {}
ChildStdout::ChildStdout(ChildStdout &&other) { *this = std::move(other); }
ChildStdout &ChildStdout::operator=(ChildStdout &&other) {
  if (this != &other)
    this->impl_ = std::move(other.impl_);
  return *this;
}
size_t ChildStdout::read(span<std::byte> buffer) {
  return Handle::read(impl_->handle, buffer);
}
size_t ChildStdout::read_to_end(std::vector<std::byte> &buffer) {
  return Handle::read_to_end(impl_->handle, buffer);
}
size_t ChildStdout::read_to_string(std::string &buffer) {
  return Handle::read_to_string(impl_->handle, buffer);
}

struct ChildStderr::Impl {
  HANDLE handle;
  Impl(HANDLE h) : handle(h) {}
};
ChildStderr::ChildStderr() : impl_(nullptr) {}
ChildStderr::~ChildStderr() {}
ChildStderr::ChildStderr(ChildStderr &&other) { *this = std::move(other); }
ChildStderr &ChildStderr::operator=(ChildStderr &&other) {
  if (this != &other)
    this->impl_ = std::move(other.impl_);
  return *this;
}
size_t ChildStderr::read(span<std::byte> buffer) {
  return Handle::read(impl_->handle, buffer);
}
size_t ChildStderr::read_to_end(std::vector<std::byte> &buffer) {
  return Handle::read_to_end(impl_->handle, buffer);
}
size_t ChildStderr::read_to_string(std::string &buffer) {
  return Handle::read_to_string(impl_->handle, buffer);
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
      SECURITY_ATTRIBUTES sa = {sizeof(SECURITY_ATTRIBUTES), NULL, TRUE};
      auto access_mode = (id == 0 ? GENERIC_READ : GENERIC_WRITE);
      HANDLE null_handle =
          CreateFile("NUL", access_mode, FILE_SHARE_READ | FILE_SHARE_WRITE,
                     &sa, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
      return {nullptr, null_handle};
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

int Child::id() { return impl_->pi.id(); }
ExitStatus Child::wait() {
  int code = impl_->pi.wait();
  ExitStatus status;
  status.impl_->code = code;
  return status;
}
void Child::kill() { impl_->pi.kill(); }
Output Child::wait_with_output() {
  Output output;
  if (io_stdout and not io_stderr) {
    this->io_stdout->read_to_string(output.std_out);
  } else if (not io_stdout and io_stderr) {
    this->io_stderr->read_to_string(output.std_err);
  } else if (io_stdout and io_stderr) {
    Handle::read2_to_string(io_stdout->impl_->handle, output.std_out,
                            io_stderr->impl_->handle, output.std_err);
  } else { // nothing to capture
  }
  output.status = this->wait();
  return output;
}

/*============================================================================*/
class Command::Impl {
  string app;
  vector<string> args;
  optional<Stdio> io_stdin;
  optional<Stdio> io_stdout;
  optional<Stdio> io_stderr;
  optional<string> cwd;
  bool inherit_env;
  vector<pair<string, string>> envs;

public:
  Impl()
      : app(string()), args(vector<string>()), io_stdin(std::nullopt),
        io_stdout(std::nullopt), io_stderr(std::nullopt), inherit_env(true) {
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
  void setup_io() {
    if (not io_stdin)
      io_stdin = Stdio::inherit();
    if (not io_stdout)
      io_stdout = Stdio::inherit();
    if (not io_stderr)
      io_stderr = Stdio::inherit();
  }

  std::string find_exe_path(const std::string &name) {
    char path[MAX_PATH];
    if (SearchPath(nullptr, name.c_str(), NULL, MAX_PATH, path, NULL) == 0) {
      throw std::runtime_error("program not found");
    }
    return std::string(path);
  }
  string build_arg() {
    string arg(this->app);
    if (!args.empty()) {
      for (int i = 0; i < args.size(); i += 1) {
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
    auto [our_stdin, their_stdin] = io_stdin->impl_->to_handles(0);    // w, r
    auto [our_stdout, their_stdout] = io_stdout->impl_->to_handles(1); // r, w
    auto [our_stderr, their_stderr] = io_stderr->impl_->to_handles(2); // r, w
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
Child Command::spawn() {
  impl_->setup_io();
  return impl_->spawn();
}
ExitStatus Command::status() {
  impl_->setup_io();
  Child child = impl_->spawn();
  return child.wait();
}

Output Command::output() {
  impl_->setup_io();
  Child child = impl_->spawn();
  return child.wait_with_output();
}
} // namespace process

#endif
