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

#include "../cli/cli.hpp"

SystemProcess::SystemProcess(std::string const cmdline) : cmdline(cmdline) {}

ProcessDispatchStatus SystemProcess::dispatch_process() {
  // terminal spoofing.
  struct winsize win_size;
  struct termios termios_p;
  // todo: error handling.
  ioctl(STDOUT_FILENO, TIOCGWINSZ, &win_size);
  tcgetattr(STDOUT_FILENO, &termios_p);

  int fd_master, fd_slave;
  openpty(&fd_master, &fd_slave, NULL, &termios_p, &win_size);
  this->fd_master = fd_master;

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

ProcessReadStatus SystemProcess::read_output(std::string &out) {
  int wstatus;
  pid_t status = waitpid(this->pid, &wstatus, WNOHANG);
  if (0 > status)
    return ProcessReadStatus::InternalError;

  char buffer[BUFSIZ] = {0};
  if (0 < read(this->fd_master, buffer, sizeof(buffer) / sizeof(buffer[0])))
    out += std::string(buffer);

  int unused;
  if (!status || !WIFEXITED(wstatus))
    return ProcessReadStatus::DataRead;

  if (WIFEXITED(wstatus) && 0 != WEXITSTATUS(wstatus))
    return ProcessReadStatus::ExitFailure;
  else
    return ProcessReadStatus::ExitSuccess;
}
#endif
