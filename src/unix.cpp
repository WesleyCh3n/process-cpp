#ifndef _WIN32
#include "process.hpp"

#include <iostream>
#include <optional>
#include <unistd.h>

namespace process {
using std::optional;
using std::pair;
using std::string;
using std::vector;

/*============================================================================*/
struct Process {
  pid_t pid;
  int wait() {
    int wstatus;
    int exit_code;
    if (waitpid(pid, &wstatus, 0) == -1) {
      throw std::runtime_error("Failed to wait for child process");
    }
    if (WIFEXITED(wstatus)) {
      exit_code = WEXITSTATUS(wstatus);
    }
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
  // me, child
  pair<optional<int>, optional<int>> to_fds(bool is_stdin = false) {
    switch (value) {
    case Value::Inherit:
      return {std::nullopt, std::nullopt};
    case Value::NewPipe:
      int fds[2];
      if (::pipe(fds) == -1)
        throw std::runtime_error("Failed to create pipe");

      return is_stdin ? std::make_pair(fds[0], fds[1])
                      : std::make_pair(fds[0], fds[1]);
    case Value::FromPipe:
      return {std::nullopt, std::nullopt};
    case Value::Null:
      return {std::nullopt, std::nullopt};
    default:
      return {std::nullopt, std::nullopt};
    }
  }
};

Stdio Stdio::pipe() { return Stdio(Value::NewPipe); }
Stdio Stdio::inherit() { return Stdio(Value::Inherit); }
Stdio Stdio::null() { return Stdio(Value::Null); }

/*============================================================================*/
ssize_t read_fd(const int &fd, char buffer[], size_t size) {
  ssize_t bytes_read;
  if ((bytes_read = ::read(fd, buffer, size - 1)) > 0) {
    buffer[bytes_read] = '\0';
    return bytes_read;
  }
  return -1;
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
  int fd;
  Impl(int fd) : fd(fd) {}
  ssize_t write(char buffer[], size_t size) {
    throw std::logic_error("unimplemented");
    return read_fd(this->fd, buffer, size);
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
  int fd;
  Impl(int fd) : fd(fd) {}
  ssize_t read(char buffer[], size_t size) {
    return read_fd(this->fd, buffer, size);
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
  int fd;
  Impl(int fd) : fd(fd) {}
  ssize_t read(char buffer[], size_t size) {
    return read_fd(this->fd, buffer, size);
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

  Impl(Process pi) : pi(pi) {}

  int id() { return pi.pid; }
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
        io_stdout(Stdio::inherit()), io_stderr(Stdio::inherit()) {}
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
    auto [our_stdin, their_stdin] = io_stdin.impl_->to_fds(true);
    auto [our_stdout, their_stdout] = io_stdout.impl_->to_fds();
    auto [our_stderr, their_stderr] = io_stderr.impl_->to_fds();
    pid_t pid = fork();
    if (pid == -1)
      throw std::runtime_error("Failed to fork");
    if (pid == 0) {
      if (auto fd = their_stdin) {
        dup2(*fd, STDIN_FILENO);
        close(*fd);
      }
      if (auto fd = their_stdout) {
        dup2(*fd, STDOUT_FILENO);
        close(*fd);
      }
      if (auto fd = their_stderr) {
        dup2(*fd, STDERR_FILENO);
        close(*fd);
      }
      if (app.empty())
        execl("/bin/sh", "sh", "-c", arg.c_str(), nullptr);
      else
        execl(app.c_str(), arg.c_str(), nullptr);
      _exit(EXIT_FAILURE);
    }
    if (auto fd = their_stdout) {
      close(*fd);
    }
    if (auto fd = their_stderr) {
      close(*fd);
    }

    Child s;
    if (our_stdin.has_value()) {
      s.io_stdin = std::make_optional<ChildStdin>();
      s.io_stdin->impl_ = std::make_unique<ChildStdin::Impl>(*our_stdin);
    }
    if (our_stdout.has_value()) {
      s.io_stdout = std::make_optional<ChildStdout>();
      s.io_stdout->impl_ = std::make_unique<ChildStdout::Impl>(*our_stdout);
    }
    if (our_stderr.has_value()) {
      s.io_stderr = std::make_optional<ChildStderr>();
      s.io_stderr->impl_ = std::make_unique<ChildStderr::Impl>(*our_stderr);
    }

    s.impl_ = std::make_unique<Child::Impl>(Process{.pid = pid});
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
