#include "environment.hpp"
#include "../kal/terminal.hpp"

/* the interactive cli requires a number of ansi escape codes, and ideally these
 * should be looked up in a terminal database using the $TERM environment
 * variable. however, this might be a little overkill for qvickbuild and so if
 * stdout is marked as a tty, complete capabilities will be assumed. in the case
 * that stdout is not a tty, qvickbuild will fall back to the legacy status
 * updates. this may be useful it the output of qvickbuild is piped to a file or
 * other tool. */

CLICapabilities CLIEnvironment::detect_cli_capabilities() {
  if (KALTerminal::is_tty())
    return CLICapabilities {
      .colour = true,
      .movement = true,
    };
  else {
    return CLICapabilities {
      .colour = false,
      .movement = false,
    };
  }
}
size_t CLIEnvironment::detect_width() {
  return KALTerminal::detect_width();
}
