#ifndef CLI_RENDER_HPP
#define CLI_RENDER_HPP

#include "cli.hpp"
#include <cstddef>
#include <memory>
#include <string>
#include <variant>
#include <vector>

class CLIRenderer {
private:
  static char const *spinner_buf[6];
  static size_t frame;

  // returns tuple of substring and bytes consumed. important: bytes consumed
  // may be greater than std::string.size() since the string may be terminated
  // by a newline, in which case it is automatically trimmed.
public:
  static std::tuple<std::string, size_t>
  get_initial_rendered_characters(std::string, size_t);
private:
  static std::string wrap_with_padding(size_t, std::string);
  static std::string draw_handle(CLIEntryHandle const &);
  static std::string ensure_clear(std::string);

  static std::string move_up(size_t rows);
  static std::string clear_line();
  static std::string hide_cursor();
  static std::string show_cursor();
  static void flush();

public:
  static void draw(std::vector<std::string>,
                   std::vector<std::shared_ptr<CLIEntryHandle>>);
};

#endif
