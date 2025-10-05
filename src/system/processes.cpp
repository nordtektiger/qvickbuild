#include "processes.hpp"
#include "../cli/colour.hpp"
#include "../errors/errors.hpp"
#include "../kal/processes.hpp"
#include "../lexer/tracking.hpp"
#include <format>
#include <sys/stat.h>

PipelineJobs::ExecuteJob::ExecuteJob(
    std::string cmdline, StreamReference reference,
    std::shared_ptr<CLIEntryHandle> entry_handle) {
  this->cmdline = cmdline;
  this->reference = reference;
  this->entry_handle = entry_handle;
}

void PipelineJobs::ExecuteJob::compute_fallback() noexcept {
  SystemProcess<LaunchType::Exec> process(cmdline);
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

void PipelineJobs::ExecuteJob::compute() noexcept {
  this->entry_handle->set_status(CLIEntryStatus::Building);
  CLI::write_verbose(this->cmdline);

  SystemProcess<LaunchType::PTY> process(cmdline);
  if (process.dispatch_process() == ProcessDispatchStatus::InternalError) {
    // if pty fails, fall back to exec.
    if (CLI::is_interactive())
      CLI::write_to_log(std::format(
          "{}{}warning:{} dispatching pty failed, falling back to execv.\n",
          CLIColour::yellow(), CLIColour::bold(), CLIColour::reset()));
    return this->compute_fallback();
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
