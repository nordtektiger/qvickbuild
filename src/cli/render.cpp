#include "render.hpp"
#include <iostream>

char const *CLIRenderer::spinner_buf[6] = {"⠏ ", "⠛ ", "⠹ ", "⠼ ", "⠶ ", "⠧ "};
size_t CLIRenderer::frame = 0;

std::string CLIRenderer::wrap_with_padding(size_t padding,
                                           std::string content) {
  size_t width = CLIEnvironment::detect_width();
  std::string padding_str = std::string(padding, ' ');
  std::string formatted = padding_str;

  for (size_t i = 0; i < content.size(); i++) {
    if (i % width == 0 && i != 0) {
      formatted += '\n';
      formatted += padding_str;
    }
    formatted += content[i];
  }

  return formatted;
}

std::string CLIRenderer::draw_handle(CLIEntryHandle const &entry_handle) {
  std::string out;
  switch (entry_handle.get_status()) {
  case CLIEntryStatus::Scheduled:
    out += "⧗ ";
    break;
  case CLIEntryStatus::Running:
    out += CLIColour::cyan() +
           CLIRenderer::spinner_buf[CLIRenderer::frame % 6] +
           CLIColour::reset();
    break;
  case CLIEntryStatus::Failed:
    out += CLIColour::red() + "⨯ " + CLIColour::reset();
    break;
  case CLIEntryStatus::Finished:
    out += CLIColour::green() + "✔ " + CLIColour::reset();
    break;
  }
  out += entry_handle.get_description();
  return CLIRenderer::wrap_with_padding(0, out);
}

void CLIRenderer::draw(
    std::vector<std::string> logs,
    std::vector<std::shared_ptr<CLIEntryHandle>> entry_handles) {
  // dump cached log content.
  for (std::string const &log : logs)
    std::cout << log << std::endl;

  for (std::shared_ptr<CLIEntryHandle> const &handle_ptr : entry_handles)
    std::cout << draw_handle(*handle_ptr) << std::endl;

  CLIRenderer::frame++;
}
