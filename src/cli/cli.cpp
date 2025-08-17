#include "cli.hpp"
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
}

void CLIEntryHandle::set_status(CLIEntryStatus status) {
  this->status = status;
}

CLIEntryStatus CLIEntryHandle::get_status() const { return this->status; }

std::string CLIEntryHandle::get_description() const {
  return this->description;
}

std::vector<LogEntry> CLI::log_buffer = {};
std::vector<std::shared_ptr<CLIEntryHandle>> CLI::entry_handles = {};
std::thread CLI::io_thread = std::thread();
std::mutex CLI::io_lock = std::mutex();
std::atomic_bool CLI::stop = false;
CLIOptions CLI::cli_options = CLIOptions();

std::shared_ptr<CLIEntryHandle>
CLI::generate_entry_handle(std::string description, CLIEntryStatus status) {
  std::shared_ptr<CLIEntryHandle> handle_ptr =
      std::make_shared<CLIEntryHandle>(description, std::nullopt, status);
  CLI::entry_handles.push_back(handle_ptr);
  return handle_ptr;
}

std::shared_ptr<CLIEntryHandle>
CLI::derive_entry_handle_from(std::shared_ptr<CLIEntryHandle> parent,
                              std::string description, CLIEntryStatus status) {
  auto handle_ptr =
      std::make_shared<CLIEntryHandle>(description, parent, status);
  parent->children.push_back(handle_ptr);
  return handle_ptr;
}

// tree search.
std::shared_ptr<CLIEntryHandle>
CLI::get_entry_handle_from_description(std::string description) {
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
  if (CLI::io_thread.joinable())
    io_thread.join();
}

void CLI::write_to_log(std::string content) { CLI::write_quiet(content); }

void CLI::write_quiet(std::string content) {
  CLI::log_buffer.push_back(LogEntry{LogLevel::Quiet, content});
}

void CLI::write_standard(std::string content) {
  CLI::log_buffer.push_back(LogEntry{LogLevel::Standard, content});
}

void CLI::write_verbose(std::string content) {
  CLI::log_buffer.push_back(LogEntry{LogLevel::Verbose, content});
}

void CLI::run() {
  while (!CLI::stop) {
    auto start_time = std::chrono::high_resolution_clock::now();

    std::vector<std::string> logs;
    for (LogEntry const &log_entry : CLI::log_buffer)
      if (log_entry.log_level <= CLI::cli_options.log_level)
        logs.push_back(log_entry.content);

    CLIRenderer::draw(logs, CLI::entry_handles);

    auto stop_time = std::chrono::high_resolution_clock::now();
    auto delta = std::chrono::duration_cast<std::chrono::milliseconds>(
        stop_time - start_time);
    std::this_thread::sleep_for(DRAW_TIMEOUT - delta);
  }
}
