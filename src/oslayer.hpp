#ifndef OSLAYER_H
#define OSLAYER_H

#include "cli/cli.hpp"
#include "pipeline.hpp"
#include "tracking.hpp"
#include <optional>
#include <string>

namespace PipelineJobs {
class ExecuteJob : public PipelineJob {
private:
  std::string cmdline;
  StreamReference reference;
  std::shared_ptr<CLIEntryHandle> entry_handle;

public:
  ExecuteJob() = delete;
  ExecuteJob(std::string, StreamReference,
             std::shared_ptr<CLIEntryHandle>);
  void compute() noexcept;
};
} // namespace PipelineJobs

class OSLayer {
public:
  static std::optional<size_t> get_file_timestamp(std::string path);
};

#endif
