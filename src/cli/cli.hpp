#ifndef CLI_HPP
#define CLI_HPP

class CLIEntryHandle;

#include "colour.hpp"
#include "environment.hpp"
#include "render.hpp"
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
  Finished,
  Failed,
};

class CLIEntryHandle {
  friend class CLI;
  friend class CLIRenderer;

private:
  std::string description;
  std::optional<std::weak_ptr<CLIEntryHandle>> parent;
  std::vector<std::shared_ptr<CLIEntryHandle>> children;
  CLIEntryStatus status;
  void set_status_internal(CLIEntryStatus);

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
  friend class CLIEntryHandle;
  friend class CLIRenderer;

private:
  static CLIOptions cli_options;
  static std::vector<LogEntry> log_buffer;
  static std::vector<std::shared_ptr<CLIEntryHandle>> entry_handles;
  static std::optional<std::shared_ptr<CLIEntryHandle>>
      search_handle_recursive(std::string, std::shared_ptr<CLIEntryHandle>);
  // static void destroy_entry_recursive(std::shared_ptr<CLIEntryHandle>,
  //                                     std::shared_ptr<CLIEntryHandle>);

  static std::thread io_thread;
  static std::mutex io_lock;
  static std::atomic_bool stop;
  static void run();

  static size_t compute_percentage_done();
  static size_t get_tasks_scheduled();
  static size_t get_tasks_compiled();
  static size_t get_tasks_skipped();

  // static size_t tasks_skipped;

public:
  static std::shared_ptr<CLIEntryHandle> generate_entry(std::string,
                                                        CLIEntryStatus);
  static std::shared_ptr<CLIEntryHandle>
      derive_entry_from(std::shared_ptr<CLIEntryHandle>, std::string,
                        CLIEntryStatus);
  static std::shared_ptr<CLIEntryHandle>
      get_entry_from_description(std::string);
  // static void destroy_entry(std::shared_ptr<CLIEntryHandle>);

  static void write_to_log(std::string);
  static void write_verbose(std::string);
  static void write_standard(std::string);
  static void write_quiet(std::string);

  static void increment_skipped_tasks();

  static void initialize(CLIOptions);
  static void stop_sync();
};

#endif
