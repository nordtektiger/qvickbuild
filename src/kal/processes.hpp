#ifndef KAL_PROCESSES_HPP
#define KAL_PROCESSES_HPP

#include "platform.hpp"
#include <string>

#if defined(kal_linux) || defined(kal_apple)
#include <unistd.h>
#endif

enum class ProcessDispatchStatus {
  Dispatched,    /* process was launched */
  InternalError, /* qvickbuild cannot proceed */
};

enum class ProcessReadStatus {
  DataRead,     /* successfully read stream */
  ExitSuccess,  /* process has exited normally */
  ExitFailure,  /* process has failed */
  InternalError /* qvickbuild cannot proceed */
};

class SystemProcess {
private:
  std::string const cmdline;
#if defined(kal_linux) || defined(kal_apple)
  pid_t pid;
  int fd_master;
  FILE *stream;
#endif

public:
  SystemProcess() = delete;
  explicit SystemProcess(std::string const);

  ProcessDispatchStatus dispatch_process();
  ProcessReadStatus read_output(std::string &);
};

#endif
