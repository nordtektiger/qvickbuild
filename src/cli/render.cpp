#include "render.hpp"
#include "colour.hpp"
#include <cassert>
#include <iostream>
#include <ranges>

class Counted {
private:
  static size_t count;

public:
  static size_t reset();
  static std::string count_str(std::string);
};

size_t Counted::count = 0;

size_t Counted::reset() {
  size_t _count = Counted::count;
  Counted::count = 0;
  return _count;
}

std::string Counted::count_str(std::string content) {
  Counted::count += std::count_if(content.begin(), content.end(),
                                  [](char c) { return c == '\n'; });
  return content;
}

char const *CLIRenderer::spinner_buf[6] = {"⠏ ", "⠛ ", "⠹ ", "⠼ ", "⠶ ", "⠧ "};
size_t CLIRenderer::frame = 0;
bool CLIRenderer::is_interactive = false;

std::string CLIRenderer::move_up(size_t rows) {
  if (rows == 0)
    return "";
  return "\033[" + std::to_string(rows) + "A";
}

std::string CLIRenderer::clear_line() { return "\033[2K"; }

std::string CLIRenderer::hide_cursor() { return "\033[?25l"; }

std::string CLIRenderer::show_cursor() { return "\033[?25h"; }

void CLIRenderer::flush() {
  std::flush(std::cout);
  std::flush(std::cerr);
}

std::tuple<std::string, size_t>
CLIRenderer::get_initial_rendered_characters(std::string str,
                                             size_t max_width) {
  size_t count = 0; // rendered characters.
  size_t i = 0;     // underlying string index.
  for (; i < str.size(); i++) {
    // check for non-rendered characters.
    if (str[i] == '\n') {
      return {str.substr(0, i), i + 1};
      continue;
    } else if (str[i] == '\r')
      continue;
    else if (str[i] == '\t')
      continue;
    else if (str[i] == '\033') {
      // handle ansi escape sequences.
      do {
        i++;
      } while (!std::isalpha(str[i]));
      // i will be incremented once more at the top of the loop.
      continue;
    }

    // check that we're not overstepping our maximum width.
    if (count >= max_width)
      break;

    // if we get here, the character should render.
    count++;
  }
  return {str.substr(0, i), i};
}

std::string CLIRenderer::wrap_with_padding(size_t padding,
                                           std::string content) {
  size_t width = CLIEnvironment::detect_width();
  std::string padding_str = std::string(padding, ' ');
  std::string formatted /* = padding_str */;

  std::string _original = content;

  std::string line_buffer;
  size_t bytes_consumed;

  std::tie(line_buffer, bytes_consumed) =
      CLIRenderer::get_initial_rendered_characters(content, width);

  while (bytes_consumed != 0) {
    formatted += padding_str + line_buffer + "\n";
    content = content.substr(bytes_consumed, content.size() - bytes_consumed);
    std::tie(line_buffer, bytes_consumed) =
        CLIRenderer::get_initial_rendered_characters(content, width);
  }

  // trim last newline to comply with old behaviour.
  if (!formatted.empty() && formatted[formatted.size() - 1] == '\n')
    formatted = formatted.substr(0, formatted.size() - 1);

  return formatted + _original.substr(0, 0);
}

std::string CLIRenderer::draw_handle_head(CLIEntryHandle const &entry_handle) {
  std::string out;
  switch (entry_handle.get_status()) {
  case CLIEntryStatus::Scheduled:
    out += "… ";
    break;
  case CLIEntryStatus::Building:
    out += CLIColour::cyan() +
           CLIRenderer::spinner_buf[CLIRenderer::frame % 6] +
           CLIColour::reset();
    break;
  case CLIEntryStatus::Failed:
    out += CLIColour::red() + "⨯ " + CLIColour::reset();
    break;
  case CLIEntryStatus::Finished:
    out += CLIColour::green() + "✓ " + CLIColour::reset();
    break;
  }
  if (entry_handle.highlighted)
    out += CLIColour::bold();
  out += entry_handle.get_description();
  out += CLIColour::reset();

  return out;
}

std::string CLIRenderer::draw_handle(CLIEntryHandle const &entry_handle) {
  std::string out;

  if (entry_handle.visible)
    out += CLIRenderer::draw_handle_head(entry_handle) + "\n";

  // sort child entires based on status.
  std::vector<std::shared_ptr<CLIEntryHandle>> entry_children =
      entry_handle.children;
  std::sort(
      entry_children.begin(), entry_children.end(),
      [](std::shared_ptr<CLIEntryHandle> a, std::shared_ptr<CLIEntryHandle> b) {
        if (a->get_status() == CLIEntryStatus::Finished &&
            a->get_status() == b->get_status())
          return a->get_time_finished() < b->get_time_finished();
        else
          return static_cast<int>(a->get_status()) >
                 static_cast<int>(b->get_status());
      });

  // draw child entries.
  int entry_children_padding = entry_handle.visible ? 2 : 0;
  for (std::shared_ptr<CLIEntryHandle> const &child_handle : entry_children) {
    std::string child_buffer = CLIRenderer::wrap_with_padding(
        entry_children_padding, CLIRenderer::draw_handle(*child_handle));
    if (!child_buffer.empty())
      out += child_buffer + "\n";
  }

  return CLIRenderer::wrap_with_padding(0, out);
}

std::string CLIRenderer::ensure_clear(std::string content) {
  std::string out = CLIRenderer::clear_line();
  for (char const &c : content | std::views::take(content.size() - 1)) {
    out += c;
    if (c == '\n')
      out += CLIRenderer::clear_line();
  }
  return out + content[content.size() - 1];
}

void CLIRenderer::draw_interactive(
    std::vector<std::string> logs, std::vector<std::string> suffix,
    std::vector<std::shared_ptr<CLIEntryHandle>> entry_handles) {
  // reset drawing position.
  std::string text_buffer = CLIRenderer::hide_cursor();
  text_buffer += CLIRenderer::move_up(Counted::reset()) + "\r";

  // dump cached log content.
  for (std::string const &log : logs) {
    text_buffer += CLIRenderer::ensure_clear(log);
  }

  // draw handles.
  for (std::shared_ptr<CLIEntryHandle> const &handle_ptr : entry_handles) {
    text_buffer += CLIRenderer::ensure_clear(
        Counted::count_str(CLIRenderer::draw_handle(*handle_ptr) + "\n"));
  }

  // draw status.
  text_buffer += CLIRenderer::ensure_clear(Counted::count_str(
      CLIRenderer::wrap_with_padding(
          0, CLIColour::bold() + "[" + CLIColour::green() +
                 std::to_string(CLI::compute_percentage_done()) + "%" +
                 CLIColour::reset() + CLIColour::bold() + "] built " +
                 CLIColour::cyan() + std::to_string(CLI::get_tasks_compiled()) +
                 CLIColour::reset() + CLIColour::bold() + " tasks" + " (" +
                 CLIColour::cyan() + std::to_string(CLI::get_tasks_skipped()) +
                 CLIColour::reset() + CLIColour::bold() + " skipped)" +
                 CLIColour::reset()) +
      "\n"));

  // draw suffix.
  for (std::string const &log : suffix)
    text_buffer += CLIRenderer::ensure_clear(
        Counted::count_str(CLIRenderer::wrap_with_padding(0, log) + "\n"));

  text_buffer += CLIRenderer::show_cursor();

  // flush to terminal.
  std::cout << text_buffer;
  CLIRenderer::flush();
  CLIRenderer::frame++;
}

void CLIRenderer::draw_legacy(std::vector<std::string> logs,
                              std::vector<std::string> suffix) {
  for (std::string const &log : logs) {
    std::cout << log;
  }
  for (std::string const &log : suffix) {
    std::cout << log;
  }
  CLIRenderer::flush();
}

void CLIRenderer::draw(
    std::vector<std::string> logs, std::vector<std::string> suffix,
    std::vector<std::shared_ptr<CLIEntryHandle>> entry_handles) {
  if (CLIRenderer::is_interactive)
    CLIRenderer::draw_interactive(logs, suffix, entry_handles);
  else
    CLIRenderer::draw_legacy(logs, suffix);
}

void CLIRenderer::set_interactive(bool is_interactive) {
  CLIRenderer::is_interactive = is_interactive;
}
