#include "colour.hpp"

// start in legacy mode.
bool CLIColour::use_formatting = false;

void CLIColour::set_formatting(bool use_formatting) {
  CLIColour::use_formatting = use_formatting;
}

std::string CLIColour::green() {
  return CLIColour::use_formatting ? "\033[32m" : "";
}

std::string CLIColour::red() {
  return CLIColour::use_formatting ? "\033[91m" : "";
}

std::string CLIColour::yellow() {
  return CLIColour::use_formatting ? "\033[33m" : "";
}

std::string CLIColour::cyan() {
  return CLIColour::use_formatting ? "\033[36m" : "";
}

std::string CLIColour::grey() {
  return CLIColour::use_formatting ? "\033[35m" : "";
}

std::string CLIColour::bold() {
  return CLIColour::use_formatting ? "\033[1m" : "";
}

std::string CLIColour::italic() {
  return CLIColour::use_formatting ? "\033[3m" : "";
}

std::string CLIColour::underline() {
  return CLIColour::use_formatting ? "\033[4m" : "";
}

std::string CLIColour::reset() {
  return CLIColour::use_formatting ? "\033[0m" : "";
}
