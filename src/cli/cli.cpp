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
size_t CLI::frame = 0;
char const *CLI::spinner_buf[6] = {"⠏ ", "⠛ ", "⠹ ", "⠼ ", "⠶ ", "⠧ "};

std::string CLI::green() {
  return CLI::cli_options.capabilities.colour ? "\033[32m" : "";
}

std::string CLI::red() {
  return CLI::cli_options.capabilities.colour ? "\033[91m" : "";
}

std::string CLI::cyan() {
  return CLI::cli_options.capabilities.colour ? "\033[36m" : "";
}

std::string CLI::bold() {
  return CLI::cli_options.capabilities.colour ? "\033[1m" : "";
}

std::string CLI::italic() {
  return CLI::cli_options.capabilities.colour ? "\033[3m" : "";
}

std::string CLI::underline() {
  return CLI::cli_options.capabilities.colour ? "\033[4m" : "";
}

std::string CLI::reset() {
  return CLI::cli_options.capabilities.colour ? "\033[0m" : "";
}

void CLI::save_position() {
  std::cout << "\033" "7";
  fflush(stdout);
}

void CLI::restore_position() {
  std::cout << "\033" "8";
  fflush(stdout);
}

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
  CLI::save_position();
  while (!CLI::stop) {
    auto start_time = std::chrono::high_resolution_clock::now();
    CLI::draw();
    auto stop_time = std::chrono::high_resolution_clock::now();
    auto delta = std::chrono::duration_cast<std::chrono::milliseconds>(
        stop_time - start_time);
    std::this_thread::sleep_for(DRAW_TIMEOUT - delta);
  }
}

std::string CLI::wrap_with_padding(size_t padding, std::string text) {
  size_t width = CLIEnvironment::detect_width();
  std::string padding_str = std::string(padding, ' ');
  std::string formatted = padding_str;

  for (size_t i = 0; i < text.size(); i++) {
    if (i % width == 0 && i != 0) {
      formatted += '\n';
      formatted += padding_str;
    }
    formatted += text[i];
  }

  return formatted;
}

std::string CLI::draw_handle(CLIEntryHandle const &handle) {
  std::string out;
  switch (handle.status) {
  case CLIEntryStatus::Scheduled:
    out += "⧗ ";
    break;
  case CLIEntryStatus::Running:
    out += CLI::cyan() + CLI::spinner_buf[CLI::frame % 6] + CLI::reset();
    break;
  case CLIEntryStatus::Failed:
    out += CLI::red() + "⨯ " + CLI::reset();
    break;
  case CLIEntryStatus::Finished:
    out += CLI::green() + "✔ " + CLI::reset();
    break;
  }
  out += handle.get_description();
  return out;
}

void CLI::draw() {
  CLI::restore_position();

  // dump cached log content
  for (LogEntry const &log : CLI::log_buffer)
    if (log.log_level <= CLI::cli_options.log_level)
      std::cout << log.content << std::endl;
  CLI::log_buffer.clear();

  CLI::save_position();

  // draw build status of tasks, todo: handle legacy logging mode
  for (std::shared_ptr<CLIEntryHandle> const &handle : CLI::entry_handles) {
    std::cout << CLI::wrap_with_padding(0, CLI::draw_handle(*handle)) << std::endl;
  }

  // draw bottom status bar

  CLI::frame++;
}
