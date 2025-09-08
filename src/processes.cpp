#include "processes.hpp"
#include "errors.hpp"
#include "kal/processes.hpp"
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

  SystemProcess process(cmdline);
  if (process.dispatch_process() == ProcessDispatchStatus::InternalError) {
    ErrorHandler::soft_report(EProcessInternal{cmdline, reference});
    this->report_error();
    return;
  }

  ProcessReadStatus status;
  std::string buffer;

  do {
    status = process.read_output(buffer);
    if (!buffer.empty()) {
      CLI::write_to_log(buffer);
      buffer.clear();
    }
  } while (status == ProcessReadStatus::DataRead);
  if (!buffer.empty())
    CLI::write_to_log(buffer);

  if (status == ProcessReadStatus::ExitFailure) {
    ErrorHandler::soft_report(ENonZeroProcess{cmdline, reference});
    this->report_error();
  } else if (status == ProcessReadStatus::InternalError) {
    ErrorHandler::soft_report(EProcessInternal{cmdline, reference});
    this->report_error();
  }
}
