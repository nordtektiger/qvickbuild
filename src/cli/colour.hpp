#ifndef CLI_FORMATTING_HPP
#define CLI_FORMATTING_HPP

#include <string>

class CLIColour {
private:
  static bool use_formatting;

public:
  static void set_formatting(bool);

  static std::string green();
  static std::string red();
  static std::string cyan();
  static std::string grey();

  static std::string bold();
  static std::string italic();
  static std::string underline();
  static std::string reset();
};

#endif
