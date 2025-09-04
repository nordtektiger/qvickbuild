#ifndef FILESYSTEM_HPP
#define FILESYSTEM_HPP

#include <optional>
#include <stddef.h>
#include <string>

namespace Filesystem {
std::optional<size_t> get_file_timestamp(std::string);
}

#endif
