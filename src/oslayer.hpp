#ifndef OSLAYER_H
#define OSLAYER_H

#include "tracking.hpp"
#include "errors.hpp"
#include "lexer.hpp"
#include <atomic>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

struct Command {
  std::string cmdline;
  StreamReference reference;
};

class OSLayer {
private:
  bool silent;
  bool parallel;

  std::vector<Command> queue = {};
  std::vector<Command> non_zero_commands;
  std::mutex error_lock;

  void _execute_command(Command command);

  void _execute_queue_sync();
  void _execute_queue_parallel();

public:
  OSLayer(bool parallel, bool silent);
  void queue_command(Command command);
  void execute_queue();
  std::vector<Command> get_non_zero_commands();

  static std::optional<size_t> get_file_timestamp(std::string path);
};

#endif
