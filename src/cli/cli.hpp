#ifndef CLI_HPP
#define CLI_HPP

class CLIEntryHandle;

#include "environment.hpp"
#include "render.hpp"
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#define DRAW_TIMEOUT std::chrono::milliseconds(50)

enum class CLIEntryStatus {
  Scheduled,
  Building,
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
  bool visible; /* cannot be changed after creation */
  std::chrono::time_point<std::chrono::high_resolution_clock> time_finished;
  bool highlighted;

public:
  CLIEntryHandle() = delete;
  explicit CLIEntryHandle(std::string,
                          std::optional<std::shared_ptr<CLIEntryHandle>>,
                          CLIEntryStatus, bool);

  void set_status(CLIEntryStatus);
  void set_highlighted(bool);
  CLIEntryStatus get_status() const;
  bool get_highlighted() const;
  std::string get_description() const;
  std::chrono::time_point<std::chrono::high_resolution_clock>
  get_time_finished() const;
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
  static std::vector<std::string> suffix_buffer;
  static std::vector<std::shared_ptr<CLIEntryHandle>> entry_handles;

  static std::mutex io_modify_lock;

  static std::thread io_thread;
  static std::mutex io_wake_lock;
  static std::condition_variable io_wake_condition;
  static std::atomic_bool io_wake_redraw;
  static std::atomic_bool stop;
  static void run();
  static void wake_for_redraw();

  static size_t compute_percentage_done();
  static size_t get_tasks_scheduled();
  static size_t get_tasks_scheduled(CLIEntryHandle);
  static size_t get_tasks_compiled();
  static size_t get_tasks_compiled(CLIEntryHandle);
  static size_t get_tasks_skipped();

  static void legacy_update_status(CLIEntryHandle const &);

  static size_t tasks_skipped;

public:
  static std::shared_ptr<CLIEntryHandle> generate_entry(std::string,
                                                        CLIEntryStatus, bool);
  static std::shared_ptr<CLIEntryHandle>
  derive_entry_from(std::shared_ptr<CLIEntryHandle>, std::string,
                    CLIEntryStatus, bool);

  static void write_to_log(std::string);
  static void write_verbose(std::string);
  static void write_standard(std::string);
  static void write_quiet(std::string);

  static void write_to_suffix(std::string);

  static void increment_skipped_tasks();

  static bool is_interactive();
  static void initialize(CLIOptions);
  static void stop_sync();
};

#endif
