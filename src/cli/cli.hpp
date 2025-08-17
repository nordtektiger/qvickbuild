#ifndef CLI_HPP
#define CLI_HPP

class CLIEntryHandle;

#include "render.hpp"
#include "colour.hpp"
#include "environment.hpp"
#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#define DRAW_TIMEOUT std::chrono::milliseconds(70)

enum class CLIEntryStatus {
  Scheduled,
  Running,
  Failed,
  Finished,
};

class CLIEntryHandle {
  friend class CLI;

private:
  std::string description;
  std::optional<std::shared_ptr<CLIEntryHandle>> parent;
  std::vector<std::shared_ptr<CLIEntryHandle>> children;
  CLIEntryStatus status;

public:
  CLIEntryHandle() = delete;
  explicit CLIEntryHandle(std::string,
                          std::optional<std::shared_ptr<CLIEntryHandle>>,
                          CLIEntryStatus);

  void set_status(CLIEntryStatus);
  CLIEntryStatus get_status() const;
  std::string get_description() const;
};

enum class LogLevel {
  Quiet,
  Standard,
  Verbose,
};

struct CLIOptions {
  LogLevel log_level;
  CLICapabilities capabilities;
};

struct LogEntry {
  LogLevel log_level;
  std::string content;
};

class CLI {
private:
  static CLIOptions cli_options;
  static std::vector<LogEntry> log_buffer;
  static std::vector<std::shared_ptr<CLIEntryHandle>> entry_handles;
  static std::optional<std::shared_ptr<CLIEntryHandle>>
      search_handle_recursive(std::string, std::shared_ptr<CLIEntryHandle>);

  static std::thread io_thread;
  static std::mutex io_lock;
  static std::atomic_bool stop;

  static void run();

public:
  static std::shared_ptr<CLIEntryHandle> generate_entry_handle(std::string,
                                                               CLIEntryStatus);
  static std::shared_ptr<CLIEntryHandle>
      derive_entry_handle_from(std::shared_ptr<CLIEntryHandle>, std::string,
                               CLIEntryStatus);
  static std::shared_ptr<CLIEntryHandle>
      get_entry_handle_from_description(std::string);
  static void write_to_log(std::string);
  static void write_verbose(std::string);
  static void write_standard(std::string);
  static void write_quiet(std::string);

  static void initialize(CLIOptions);
  static void stop_sync();
};

#endif
