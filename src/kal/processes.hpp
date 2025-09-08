#ifndef KAL_PROCESSES_HPP
#define KAL_PROCESSES_HPP

#include "platform.hpp"
#include <string>

#if defined(kal_linux) || defined(kal_apple)
#include <unistd.h>
#endif

enum class ReadStatus {
  DataRead, /* successfully read stream */
  ExitSuccess, /* process has exited normally */
  ExitFailure, /* process has failed */
};

class SystemProcess {
private:
#if defined(kal_linux) || defined(kal_apple)
  pid_t pid;
  int descriptors[2];
  FILE *stream;
#endif

public:
  SystemProcess() = delete;
  explicit SystemProcess(std::string);

  ReadStatus read_output(std::string &);
};

#endif
