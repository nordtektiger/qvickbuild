#include "filesystem.hpp"
#include <sys/stat.h>

// this might incorrectly modify struct name.
#ifdef WIN32
#define stat _stat
#endif

// account for darwin naming conventions.
#ifdef __APPLE__
#define ST_CTIME st_ctimespec.tv_sec
#else
#define ST_CTIME st_ctime
#endif

std::optional<size_t> Filesystem::get_file_timestamp(std::string path) {
  struct stat t_stat;
  if (0 > stat(path.c_str(), &t_stat))
    return std::nullopt;
  return t_stat.ST_CTIME;
}
