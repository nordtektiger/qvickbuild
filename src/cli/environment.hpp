#ifndef CLI_ENVIRONMENT_HPP
#define CLI_ENVIRONMENT_HPP

#include <cstddef>

struct CLICapabilities {
  bool colour;
  bool movement;
};

namespace CLIEnvironment {
  CLICapabilities detect_cli_capabilities();
  size_t detect_width();
}

#endif
