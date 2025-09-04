#include "oslayer.hpp"
#include "errors.hpp"
#include "tracking.hpp"
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

PipelineJobs::ExecuteJob::ExecuteJob(
    std::string cmdline, StreamReference reference,
    std::shared_ptr<CLIEntryHandle> entry_handle) {
  this->cmdline = cmdline;
  this->reference = reference;
  this->entry_handle = entry_handle;
}

void PipelineJobs::ExecuteJob::compute() noexcept {
  this->entry_handle->set_status(CLIEntryStatus::Building);
  CLI::write_verbose(this->cmdline);
  int code = system(this->cmdline.c_str());
  if (0 != code) {
    this->report_error();
    ErrorHandler::soft_report(ENonZeroProcess{cmdline, reference});
  }
}

std::optional<size_t> OSLayer::get_file_timestamp(std::string path) {
  struct stat t_stat;
  if (0 > stat(path.c_str(), &t_stat))
    return std::nullopt;
  return t_stat.ST_CTIME;
}
