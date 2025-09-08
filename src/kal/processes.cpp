#include "processes.hpp"
#include "platform.hpp"
#include <iostream>

// todo: win32: subprocess management
#if defined(kal_linux) || defined(kal_apple)
#include <cassert>
#include <cstring>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

SystemProcess::SystemProcess(std::string const cmdline) : cmdline(cmdline) {}

ProcessDispatchStatus SystemProcess::dispatch_process() {
  // setup.
  if (0 > pipe(this->descriptors))
    return ProcessDispatchStatus::InternalError;

  // fork.
  this->pid = fork();
  if (0 > this->pid)
    return ProcessDispatchStatus::InternalError;

  if (0 == pid) {
    // subprocess.
    close(this->descriptors[0]);
    dup2(this->descriptors[1], STDOUT_FILENO);
    dup2(this->descriptors[1], STDERR_FILENO);

    std::vector<char const *> args = {"/bin/sh", "-c", cmdline.c_str(), NULL};
    execv("/bin/sh", const_cast<char *const *>(args.data()));
    exit(-1); // we are in a duplicate interpreter instance, and so we need to
              // completely abandon ship here. if we return, we'd get two
              // qvickbuild instances attempting to evaluate the same
              // configuration at the same time.

  } else {
    // interpreter.
    close(this->descriptors[1]);
    this->stream = fdopen(this->descriptors[0], "r");
    return ProcessDispatchStatus::Dispatched;
  }
  assert(false && "invalid fork return code");
}

ProcessReadStatus SystemProcess::read_output(std::string &out) {
  char buffer[BUFSIZ] = {0};
  if (fgets(buffer, BUFSIZ, this->stream) != NULL && std::strlen(buffer) != 0)
    out += std::string(buffer);

  int wstatus;
  pid_t status = waitpid(this->pid, &wstatus, WNOHANG);
  if (0 > status)
    return ProcessReadStatus::InternalError;

  int unused;
  if (!status || !WIFEXITED(wstatus))
    return ProcessReadStatus::DataRead;

  if (WIFEXITED(wstatus) && 0 != WEXITSTATUS(wstatus))
    return ProcessReadStatus::ExitFailure;
  else
    return ProcessReadStatus::ExitSuccess;
}
#endif
