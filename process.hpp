#pragma once

#include <optional>
#include <string>

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
  class Impl;
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
  class Impl;
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
  class Impl;
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
  class Impl;
  unique_ptr<Impl> impl_;
  friend class Command;
};

class ExitStatus {
  // private:
  //   class Impl;
  //   unique_ptr<Impl> impl_;
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
  Command();
  ~Command();
  Command(Command &&other);            // move ctor
  Command &operator=(Command &&other); // move assignment

  Command &&arg(std::string arg);
  Command &&std_out(Stdio io);
  Command &&std_err(Stdio io);
  Child spawn();

private:
  class Impl;
  unique_ptr<Impl> impl_;
};
} // namespace process
