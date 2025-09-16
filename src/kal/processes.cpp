#include "processes.hpp"
#include "platform.hpp"

// todo: win32: subprocess management
#if defined(kal_linux) || defined(kal_apple)
#include "termios.h"
#include "utmp.h"
#include <cassert>
#include <cstring>
#include <pty.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

template <typename T>
SystemProcess<T>::SystemProcess(std::string const cmdline) : cmdline(cmdline) {}
template SystemProcess<LaunchType::PTY>::SystemProcess(std::string const);
template SystemProcess<LaunchType::Exec>::SystemProcess(std::string const);

template <>
ProcessDispatchStatus SystemProcess<LaunchType::PTY>::dispatch_process() {
  // clone terminal details - this won't be exactly accurate (number of rows and
  // y height will be less due to qvickbuild output) but this is most likely
  // good enough.
  struct winsize win_size;
  struct termios termios_p;
  if (0 > ioctl(STDOUT_FILENO, TIOCGWINSZ, &win_size))
    return ProcessDispatchStatus::InternalError;
  if (0 > tcgetattr(STDOUT_FILENO, &termios_p))
    return ProcessDispatchStatus::InternalError;

  int fd_master, fd_slave;
  if (0 > openpty(&fd_master, &fd_slave, NULL, &termios_p, &win_size))
    return ProcessDispatchStatus::InternalError;

  this->fd_read = fd_master;

  // fork.
  this->pid = fork();
  if (0 > this->pid)
    return ProcessDispatchStatus::InternalError;

  if (0 == pid) {
    // subprocess.
    login_tty(fd_slave);
    close(fd_master);

    std::vector<char const *> args = {"/bin/sh", "-c", cmdline.c_str(), NULL};
    execv("/bin/sh", const_cast<char *const *>(args.data()));
    exit(-1); // we are in a duplicate interpreter instance, and so we need to
              // completely abandon ship here. if we return, we'd get two
              // qvickbuild instances attempting to evaluate the same
              // configuration at the same time.

  } else {
    // interpreter.
    close(fd_slave);
    return ProcessDispatchStatus::Dispatched;
  }
  assert(false && "invalid fork return code");
}

template <>
ProcessDispatchStatus SystemProcess<LaunchType::Exec>::dispatch_process() {
  // setup.
  int descriptors[2];
  if (0 > pipe(descriptors))
    return ProcessDispatchStatus::InternalError;

  // fork.
  this->pid = fork();
  if (0 > this->pid)
    return ProcessDispatchStatus::InternalError;

  if (0 == pid) {
    // subprocess.
    close(descriptors[0]);
    dup2(descriptors[1], STDOUT_FILENO);
    dup2(descriptors[1], STDERR_FILENO);

    std::vector<char const *> args = {"/bin/sh", "-c", cmdline.c_str(), NULL};
    execv("/bin/sh", const_cast<char *const *>(args.data()));
    exit(-1); // we are in a duplicate interpreter instance, and so we need to
              // completely abandon ship here. if we return, we'd get two
              // qvickbuild instances attempting to evaluate the same
              // configuration at the same time.

  } else {
    // interpreter.
    close(descriptors[1]);
    this->fd_read = descriptors[0];
    // this->stream = fdopen(descriptors[0], "r");
    return ProcessDispatchStatus::Dispatched;
  }
  assert(false && "invalid fork return code");
}

template <typename T>
ProcessReadStatus SystemProcess<T>::read_output(std::string &out) {
  int wstatus;
  pid_t status = waitpid(this->pid, &wstatus, WNOHANG);
  if (0 > status)
    return ProcessReadStatus::InternalError;

  char buffer[BUFSIZ] = {0};
  if (0 < read(this->fd_read, buffer, sizeof(buffer) / sizeof(buffer[0])))
    out += std::string(buffer);

  if (!status || !WIFEXITED(wstatus))
    return ProcessReadStatus::DataRead;

  if (WIFEXITED(wstatus) && 0 != WEXITSTATUS(wstatus))
    return ProcessReadStatus::ExitFailure;
  else
    return ProcessReadStatus::ExitSuccess;
}
template ProcessReadStatus
SystemProcess<LaunchType::PTY>::read_output(std::string &);
template ProcessReadStatus
SystemProcess<LaunchType::Exec>::read_output(std::string &);
#endif
