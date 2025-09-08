#include "processes.hpp"
#include "platform.hpp"
#include <iostream>

// todo: win32: subprocess management
#if defined(kal_linux) || defined(kal_apple)
#include <cassert>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>
#include <cstring>

SystemProcess::SystemProcess(std::string cmdline) {
  // setup.
  if (0 > pipe(this->descriptors))
    assert(false && "failed to create pipe: todo: proper error handling");

  // fork.
  this->pid = fork();
  assert(0 <= this->pid && "failed to fork: todo: proper error handling");

  if (0 == pid) {
    // subprocess.
    close(this->descriptors[0]);
    dup2(this->descriptors[1], STDOUT_FILENO);
    dup2(this->descriptors[1], STDERR_FILENO);

    std::vector<char const *> args = {"/bin/sh", "-c", cmdline.c_str(), NULL};
    execv("/bin/sh", const_cast<char *const *>(args.data()));
    assert(false && "failed to execv: todo: proper error handling");

  } else {
    // interpreter.
    close(this->descriptors[1]);
    this->stream = fdopen(this->descriptors[0], "r");
  }
}

ReadStatus SystemProcess::read_output(std::string &out) {
  char buffer[BUFSIZ] = {0};
  if (fgets(buffer, BUFSIZ, this->stream) != NULL &&
      std::strlen(buffer) != 0)
    out += std::string(buffer);

  int wstatus;
  pid_t status = waitpid(this->pid, &wstatus, WNOHANG);
  assert(0 <= status && "failed to waitpid: todo: proper error handling");

  int unused;
  if (!status || !WIFEXITED(wstatus))
    return ReadStatus::DataRead;

  if (WIFEXITED(wstatus) && 0 != WEXITSTATUS(wstatus))
    return ReadStatus::ExitFailure;
  else
    return ReadStatus::ExitSuccess;
}
#endif
