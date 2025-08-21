#ifndef CLI_RENDER_HPP
#define CLI_RENDER_HPP

#include "cli.hpp"
#include <cstddef>
#include <memory>
#include <string>
#include <vector>

class CLIRenderer {
private:
  static char const *spinner_buf[6];
  static size_t frame;

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
