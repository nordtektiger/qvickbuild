#include "cli.hpp"
#include <algorithm>
#include <cassert>
#include <iostream>

CLIEntryHandle::CLIEntryHandle(
    std::string description,
    std::optional<std::shared_ptr<CLIEntryHandle>> parent,
    CLIEntryStatus status) {
  this->description = description;
  this->parent = parent;
  this->children = {};
  this->status = status;
  this->highlighted = false;
}

void CLIEntryHandle::set_highlighted(bool highlighted) {
  this->highlighted = highlighted;
}

void CLIEntryHandle::set_status_internal(CLIEntryStatus status) {
  this->status = status;
  if (status == CLIEntryStatus::Finished)
    this->time_finished = std::chrono::high_resolution_clock::now();
}

void CLIEntryHandle::set_status(CLIEntryStatus status) {
  std::unique_lock<std::mutex> guard(CLI::io_modify_lock);
  this->set_status_internal(status);
}

CLIEntryStatus CLIEntryHandle::get_status() const { return this->status; }

std::string CLIEntryHandle::get_description() const {
  return this->description;
}

std::chrono::time_point<std::chrono::high_resolution_clock>
CLIEntryHandle::get_time_finished() const {
  return this->time_finished;
}

std::vector<LogEntry> CLI::log_buffer = {};
std::vector<std::shared_ptr<CLIEntryHandle>> CLI::entry_handles = {};
std::thread CLI::io_thread = std::thread();
std::mutex CLI::io_modify_lock = std::mutex();
std::mutex CLI::io_wake_lock = std::mutex();
std::condition_variable CLI::io_wake_condition = std::condition_variable();
std::atomic_bool CLI::io_wake_redraw = false;
std::atomic_bool CLI::stop = false;
size_t CLI::tasks_skipped = 0;
CLIOptions CLI::cli_options = CLIOptions();

std::shared_ptr<CLIEntryHandle> CLI::generate_entry(std::string description,
                                                    CLIEntryStatus status) {
  std::unique_lock<std::mutex> guard(CLI::io_modify_lock);
  std::shared_ptr<CLIEntryHandle> handle_ptr =
      std::make_shared<CLIEntryHandle>(description, std::nullopt, status);
  CLI::entry_handles.push_back(handle_ptr);

  // request a cli redraw.
  CLI::wake_for_redraw();

  return handle_ptr;
}

std::shared_ptr<CLIEntryHandle>
CLI::derive_entry_from(std::shared_ptr<CLIEntryHandle> parent,
                       std::string description, CLIEntryStatus status) {
  std::unique_lock<std::mutex> guard(CLI::io_modify_lock);
  auto handle_ptr =
      std::make_shared<CLIEntryHandle>(description, parent, status);
  parent->children.push_back(handle_ptr);

  // request a cli redraw.
  CLI::wake_for_redraw();

  return handle_ptr;
}

// tree search.
std::shared_ptr<CLIEntryHandle>
CLI::get_entry_from_description(std::string description) {
  std::unique_lock<std::mutex> guard(CLI::io_modify_lock);
  for (std::shared_ptr<CLIEntryHandle> const &handle_ptr : CLI::entry_handles) {
    std::optional<std::shared_ptr<CLIEntryHandle>> target;
    if ((target = CLI::search_handle_recursive(description, handle_ptr))) {
      return *target;
    }
  }
  assert(false && "attempt to get nonexistent handle from description");
}

std::optional<std::shared_ptr<CLIEntryHandle>>
CLI::search_handle_recursive(std::string description,
                             std::shared_ptr<CLIEntryHandle> handle_ptr) {
  if (handle_ptr->get_description() == description)
    return handle_ptr;
  for (std::shared_ptr<CLIEntryHandle> const &child_ptr :
       handle_ptr->children) {
    std::optional<std::shared_ptr<CLIEntryHandle>> target =
        search_handle_recursive(description, child_ptr);
    if (target)
      return target;
  }
  return std::nullopt;
}

void CLI::initialize(CLIOptions cli_options) {
  CLIColour::set_formatting(cli_options.capabilities.colour);
  CLI::cli_options = cli_options;
  CLI::stop = false;
  CLI::io_thread = std::thread(CLI::run);
}

void CLI::stop_sync() {
  CLI::stop = true;

  // request a cli redraw so that it doesn't wait for the timeout.
  CLI::wake_for_redraw();

  if (CLI::io_thread.joinable())
    io_thread.join();
}

void CLI::write_to_log(std::string content) { CLI::write_quiet(content); }

void CLI::write_quiet(std::string content) {
  // request a cli redraw.
  CLI::wake_for_redraw();

  std::unique_lock<std::mutex> guard(CLI::io_modify_lock);
  CLI::log_buffer.push_back(LogEntry{LogLevel::Quiet, content});
}

void CLI::write_standard(std::string content) {
  // request a cli redraw.
  CLI::wake_for_redraw();

  std::unique_lock<std::mutex> guard(CLI::io_modify_lock);
  CLI::log_buffer.push_back(LogEntry{LogLevel::Standard, content});
}

void CLI::write_verbose(std::string content) {
  // request a cli redraw.
  CLI::wake_for_redraw();

  std::unique_lock<std::mutex> guard(CLI::io_modify_lock);
  CLI::log_buffer.push_back(LogEntry{LogLevel::Verbose, content});
}

void CLI::increment_skipped_tasks() {
  // request a cli redraw.
  CLI::wake_for_redraw();

  CLI::tasks_skipped++;
}

size_t CLI::get_tasks_skipped() { return CLI::tasks_skipped; }
size_t CLI::get_tasks_scheduled() {
  size_t sum = 0;
  for (std::shared_ptr<CLIEntryHandle> entry : CLI::entry_handles)
    sum += get_tasks_scheduled(*entry);
  return sum;
}
size_t CLI::get_tasks_scheduled(CLIEntryHandle entry) {
  size_t sum = 1;
  for (std::shared_ptr<CLIEntryHandle> child : entry.children)
    sum += CLI::get_tasks_scheduled(*child);
  return sum;
}
size_t CLI::get_tasks_compiled() {
  size_t sum = 0;
  for (std::shared_ptr<CLIEntryHandle> entry : CLI::entry_handles)
    sum += get_tasks_compiled(*entry);
  return sum;
}
size_t CLI::get_tasks_compiled(CLIEntryHandle entry) {
  size_t sum = entry.get_status() == CLIEntryStatus::Finished;
  for (std::shared_ptr<CLIEntryHandle> child : entry.children)
    sum += get_tasks_compiled(*child);
  return sum;
}

size_t CLI::compute_percentage_done() {
  size_t tasks_scheduled = CLI::get_tasks_scheduled();
  if (tasks_scheduled == 0)
    return 0;
  return (CLI::get_tasks_compiled() * 100) / (tasks_scheduled);
}

void CLI::run() {
  while (!CLI::stop) {
    // wait until wake, or until the redraw exceeds the timeout.
    std::unique_lock<std::mutex> guard_wake(CLI::io_wake_lock);
    CLI::io_wake_condition.wait_for(guard_wake, DRAW_TIMEOUT);
    CLI::io_wake_redraw = false;
    guard_wake.unlock();

    // collect appropriate logs.
    std::unique_lock<std::mutex> guard_modify(CLI::io_modify_lock);
    std::vector<std::string> logs;
    for (LogEntry const &log_entry : CLI::log_buffer)
      if (log_entry.log_level <= CLI::cli_options.log_level)
        logs.push_back(log_entry.content);
    CLI::log_buffer.clear();

    // render frame.
    CLIRenderer::draw(logs, CLI::entry_handles);
  }
}

void CLI::wake_for_redraw() {
  std::unique_lock<std::mutex> guard(CLI::io_wake_lock);
  CLI::io_wake_redraw = true;
  guard.unlock();
  CLI::io_wake_condition.notify_one();
}
