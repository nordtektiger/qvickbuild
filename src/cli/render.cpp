#include "render.hpp"
#include <iostream>

class Counted {
private:
  static size_t count;

public:
  // enum structure inspired by prowaretech's custom std::cin and std::cout.
  // https://www.prowaretech.com/articles/current/c-plus-plus/implement-std-cin-and-std-cout
  enum stdout {
    cout,
  };
  enum stderr {
    cerr,
  };
  static size_t reset();
  friend Counted::stdout operator<<(Counted::stdout, std::string const &);
  friend Counted::stderr operator<<(Counted::stderr, std::string const &);
};

size_t Counted::count = 0;

size_t Counted::reset() {
  size_t _count = Counted::count;
  Counted::count = 0;
  return _count;
}

Counted::stdout operator<<(Counted::stdout stdout, std::string const &content) {
  Counted::count += std::count_if(content.begin(), content.end(),
                                  [](char c) { return c == '\n'; });
  std::cout << content;
  return stdout;
}

Counted::stderr operator<<(Counted::stderr stderr, std::string const &content) {
  Counted::count += std::count_if(content.begin(), content.end(),
                                  [](char c) { return c == '\n'; });
  std::cerr << content;
  return stderr;
}

char const *CLIRenderer::spinner_buf[6] = {"⠏ ", "⠛ ", "⠹ ", "⠼ ", "⠶ ", "⠧ "};
size_t CLIRenderer::frame = 0;

void CLIRenderer::move_up(size_t rows) {
  if (rows == 0)
    return;
  std::cout << "\033[" << rows << "A";
}

void CLIRenderer::clear_line() { std::cout << "\033[2K"; }

void CLIRenderer::flush() {
  std::flush(std::cout);
  std::flush(std::cerr);
}

std::string CLIRenderer::wrap_with_padding(size_t padding,
                                           std::string content) {
  size_t width = CLIEnvironment::detect_width();
  std::string padding_str = std::string(padding, ' ');
  std::string formatted = padding_str;

  size_t last_newline = -1;
  for (size_t i = 0; i < content.size(); i++) {
    if (content[i] == '\n') {
      last_newline = i;
      formatted += '\n' + padding_str;
      continue;
    } else if ((i - last_newline) % width == 0 && i != 0) {
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
    out += "⧖ ";
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
  for (std::shared_ptr<CLIEntryHandle> const &child_handle :
       entry_handle.children) {
    out += "\n" + CLIRenderer::wrap_with_padding(
                      2, CLIRenderer::draw_handle(*child_handle));
  }
  return CLIRenderer::wrap_with_padding(0, out);
}

void CLIRenderer::draw(
    std::vector<std::string> logs,
    std::vector<std::shared_ptr<CLIEntryHandle>> entry_handles) {
  // reset drawing position.
  CLIRenderer::move_up(Counted::reset());

  // dump cached log content.
  for (std::string const &log : logs) {
    CLIRenderer::clear_line();
    std::cout << log << "\n";
  }

  // draw handles.
  for (std::shared_ptr<CLIEntryHandle> const &handle_ptr : entry_handles) {
    CLIRenderer::clear_line();
    Counted::cout << draw_handle(*handle_ptr) << "\n";
  }

  // draw status.
  Counted::cout << CLIColour::green() << "building x tasks" << CLIColour::reset() << "\n";

  // flush to terminal.
  CLIRenderer::flush();
  CLIRenderer::frame++;
}
