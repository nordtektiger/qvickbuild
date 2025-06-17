#include "tracking.hpp"
#include "oslayer.hpp"
#include <sys/stat.h>

// this might incorrectly modify struct name.
#ifdef WIN32
#define stat _stat
#endif

// account for darwin naming conventions.
#ifdef __APPLE__
#define ST_CTIME st_ctimespec.tv_sec
#else
#define ST_CTIME st_ctime
#endif

OSLayer::OSLayer(bool parallel, bool silent) {
  this->parallel = parallel;
  this->silent = silent;
}

void OSLayer::queue_command(Command command) { queue.push_back(command); }

void OSLayer::execute_queue() {
  if (parallel)
    _execute_queue_parallel();
  else
    _execute_queue_sync();
}

void OSLayer::_execute_queue_parallel() {
  std::vector<std::thread> pool;
  for (Command const &command : queue) {
    pool.push_back(std::thread(&OSLayer::_execute_command, this, command));
  }
  queue = {};
  for (std::thread &thread : pool) {
    thread.join();
  }
}

void OSLayer::_execute_queue_sync() {
  for (Command const &command : queue) {
    _execute_command(command);
  }
  queue = {};
}

void OSLayer::_execute_command(Command command) {
  int code;
  if (silent)
    code = system((command.cmdline + " 1>/dev/null 2>&1").c_str());
  else
    code = system(command.cmdline.c_str());

  if (0 != code) {
    this->error_lock.lock();
    non_zero_processes.push_back(command.reference);
    this->error_lock.unlock();
  }
}

std::optional<size_t> OSLayer::get_file_timestamp(std::string path) {
  struct stat t_stat;
  if (0 > stat(path.c_str(), &t_stat))
    return std::nullopt;
  return t_stat.ST_CTIME;
}

// std::vector<ErrorContext> OSLayer::get_errors() { return this->errors; }

// #define __SHELL_SUFFIX " 2>&1"
//
// // Executes a shell command and returns the output.
// #ifdef __GNUC__
// #include <unistd.h>
// ShellResult Shell::execute(std::string cmdline) {
//   return {"", 0};
//   // Setup pipes
//   char buffer[128];
//   std::string stdout;
//
//   FILE *process = popen((cmdline + __SHELL_SUFFIX).c_str(), "r");
//   if (!process)
//     throw ShellException("Unable to execute command");
//
//   while (fgets(buffer, sizeof(buffer), process) != NULL) {
//     stdout += buffer;
//   }
//
//   int status = pclose(process);
//   return {
//       stdout,
//       status,
//   };
// }
// #elif __WIN32
// std::string Shell::execute(std::string cmdline) {
//   // TODO: IMPLEMENT THIS
//   throw ShellException("Win32 not supported yet");
// }
// #else
// #error Unsupported platform: Only Gnu and Win32 are supported
// #endif
