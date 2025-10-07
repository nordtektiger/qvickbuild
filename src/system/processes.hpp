#ifndef PROCESSES_H
#define PROCESSES_H

#include "../cli/cli.hpp"
#include "../lexer/tracking.hpp"
#include "pipeline.hpp"
#include <string>

struct ExecutionOptions {
  bool cli;
  bool silent;
};

namespace PipelineJobs {
class ExecuteJob : public PipelineJob {
private:
  std::string cmdline;
  StreamReference reference;
  std::shared_ptr<CLIEntryHandle> entry_handle;
  ExecutionOptions options;
  void compute_fallback() noexcept;

public:
  ExecuteJob() = delete;
  explicit ExecuteJob(std::string, StreamReference,
                      std::shared_ptr<CLIEntryHandle>, ExecutionOptions);
  void compute() noexcept;
};
} // namespace PipelineJobs

#endif
