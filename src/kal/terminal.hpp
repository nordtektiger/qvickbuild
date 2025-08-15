#ifndef KAL_TERMINAL_HPP
#define KAL_TERMINAL_HPP

#include "platform.hpp"
#include <cstddef>


namespace KALTerminal {
  size_t detect_width();
  bool is_tty();
}

#endif
