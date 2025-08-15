#ifndef CLI_HPP
#define CLI_HPP

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
  static std::vector<LogEntry> log_buffer;
  static std::vector<std::shared_ptr<CLIEntryHandle>> entry_handles;
  static std::optional<std::shared_ptr<CLIEntryHandle>>
      search_handle_recursive(std::string, std::shared_ptr<CLIEntryHandle>);

  static std::thread io_thread;
  static std::mutex io_lock;
  static std::atomic_bool stop;

  static std::string wrap_with_padding(size_t, std::string);
  static std::string draw_handle(CLIEntryHandle const &);
  static void draw();
  static void run();
  static size_t frame;
  static char const *spinner_buf[6];

  static void save_position();
  static void restore_position();

  static CLIOptions cli_options;

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

  static std::string green();
  static std::string red();
  static std::string cyan();

  static std::string bold();
  static std::string italic();
  static std::string underline();
  static std::string reset();
};

#endif
