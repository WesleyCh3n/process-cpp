#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#ifdef _MSC_VER
#include <basetsd.h>
typedef SSIZE_T ssize_t;
#endif

namespace process {
using std::unique_ptr;

class Stdio {
public:
  ~Stdio();
  Stdio(Stdio &&other);
  Stdio &operator=(Stdio &&other);

  enum class Value { Inherit, NewPipe, FromPipe, Null };
  static Stdio pipe();
  static Stdio inherit();
  static Stdio null();

private:
  Stdio(Value value);
  struct Impl;
  std::unique_ptr<Impl> impl_;
  friend class Command;
};

class ChildStdin {
public:
  ChildStdin();
  ~ChildStdin();
  ChildStdin(ChildStdin &&other);
  ChildStdin &operator=(ChildStdin &&other);

  ssize_t write(char buffer[], size_t size);

private:
  struct Impl;
  unique_ptr<Impl> impl_;
  friend class Command;
};

class ChildStdout {
public:
  ChildStdout();
  ~ChildStdout();
  ChildStdout(ChildStdout &&other);
  ChildStdout &operator=(ChildStdout &&other);

  ssize_t read(char buffer[], size_t size);

private:
  struct Impl;
  unique_ptr<Impl> impl_;
  friend class Command;
};

class ChildStderr {
public:
  ChildStderr();
  ~ChildStderr();
  ChildStderr(ChildStderr &&other);
  ChildStderr &operator=(ChildStderr &&other);

  ssize_t read(char buffer[], size_t size);

private:
  struct Impl;
  unique_ptr<Impl> impl_;
  friend class Command;
};

class ExitStatus {
public:
  bool success();
  std::optional<int> code();
  ExitStatus();
  ExitStatus(ExitStatus &&);
  ExitStatus &operator=(ExitStatus &&);
  ~ExitStatus();

private:
  struct Impl;
  unique_ptr<Impl> impl_;
  friend class Child;
};

struct Output {
  ExitStatus status;
  std::string std_out;
  std::string std_err;
};

class Child {
public:
  std::optional<ChildStdin> io_stdin;
  std::optional<ChildStdout> io_stdout;
  std::optional<ChildStderr> io_stderr;

  ~Child();
  Child(Child &&other);
  Child &operator=(Child &&other);

  int id();
  void kill();
  Output wait_with_output();
  ExitStatus wait();
  void try_wait();

private:
  Child();
  struct Impl;
  unique_ptr<Impl> impl_;
  friend class Command;
};

class Command {
public:
  Command(const std::string &app = std::string());
  ~Command();
  Command(Command &&other);            // move ctor
  Command &operator=(Command &&other); // move assignment

  Command &&arg(const std::string &arg);
  Command &&args(const std::vector<std::string> &arg);
  Command &&std_out(Stdio io);
  Command &&std_err(Stdio io);
  Command &&current_dir(const std::string &path);
  Command &&env(const std::string &key, const std::string &value);
  Command &&env_clear();
  ExitStatus status();
  Output output();
  Child spawn();

private:
  class Impl;
  unique_ptr<Impl> impl_;
};
} // namespace process
