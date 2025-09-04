#include "processes.hpp"
#include "errors.hpp"
#include "tracking.hpp"
#include <sys/stat.h>

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
