#include "terminal.hpp"
#include <sys/ioctl.h>
#include <unistd.h>

#include "platform.hpp"

// todo: win32: terminal detection
#if defined(kal_linux) || defined(kal_apple)
size_t KALTerminal::detect_width() {
  struct winsize win_size;
  ioctl(STDOUT_FILENO, TIOCGWINSZ, &win_size);
  if (win_size.ws_col == 0)
    return 9999;
  return win_size.ws_col;
}
#endif

// todo: win32: tty detection
#if defined(kal_linux) || defined(kal_apple)
bool KALTerminal::is_tty() {
  return isatty(STDOUT_FILENO);
}
#endif

