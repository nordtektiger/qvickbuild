#ifndef PROCESSES_H
#define PROCESSES_H

#include "cli/cli.hpp"
#include "pipeline.hpp"
#include "tracking.hpp"
#include <string>

namespace PipelineJobs {
class ExecuteJob : public PipelineJob {
private:
  std::string cmdline;
  StreamReference reference;
  std::shared_ptr<CLIEntryHandle> entry_handle;
  void compute_fallback() noexcept;

public:
  ExecuteJob() = delete;
  ExecuteJob(std::string, StreamReference, std::shared_ptr<CLIEntryHandle>);
  void compute() noexcept;
};
} // namespace PipelineJobs

#endif
