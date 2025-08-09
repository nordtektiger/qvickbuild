#ifndef OSLAYER_H
#define OSLAYER_H

#include "pipeline.hpp"
#include "tracking.hpp"
#include <atomic>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace PipelineJobs {
class ExecuteJob : public PipelineJob {
private:
  std::string cmdline;
  StreamReference reference;

public:
  ExecuteJob() = delete;
  ExecuteJob(std::string, StreamReference);
  void compute() noexcept;
};
} // namespace PipelineJobs

class OSLayer {
public:
  static std::optional<size_t> get_file_timestamp(std::string path);
};

#endif
