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

  static void move_up(size_t rows);
  static void clear_line();
  static void flush();
public:
  static std::string wrap_with_padding(size_t, std::string);
private:
  static std::string draw_handle(CLIEntryHandle const &);

public:
  static void draw(std::vector<std::string>, std::vector<std::shared_ptr<CLIEntryHandle>>);
};

#endif
