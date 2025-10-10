#ifndef PIPELINE_HPP
#define PIPELINE_HPP

#include <atomic>
#include <memory>
#include <mutex>
#include <semaphore>
#include <thread>
#include <vector>

class PipelineJob {
  friend class Pipeline;
  template <typename M> friend class PipelineScheduler;

private:
  std::binary_semaphore notifier;
  std::atomic_bool error;
  std::atomic_bool aborted;

public:
  PipelineJob() : notifier{0}, error(false), aborted(false) {};

  virtual void compute() noexcept = 0;
  void await_completion();

  void report_error();
  bool had_error() const;

  void mark_aborted();
  bool was_aborted() const;
};

namespace PipelineJobs {
class ExecuteString : public PipelineJob {
public:
  void compute() noexcept;
};
} // namespace PipelineJobs

class Pipeline {
  template <typename M> friend class PipelineScheduler;

private:
  static std::vector<std::thread> thread_pool;
  static std::atomic_bool stop_pipeline;

  static std::mutex queue_lock;
  static std::vector<std::shared_ptr<PipelineJob>> job_queue;
  static std::counting_semaphore<INT_MAX> queue_notifier;

  static void pool_loop();
  static void job_compute(std::shared_ptr<PipelineJob>);

public:
  static void push_to_queue(std::shared_ptr<PipelineJob>);
  static void execute_unbound(std::shared_ptr<PipelineJob>);
  static void initialize(size_t);
  static void stop_sync();
  static void stop_async();
  static void abort_queued();
};

enum class PipelineSchedulingTopography {
  Sequential,
  Parallel,
};

namespace PipelineSchedulingMethod {
struct Managed {};
struct Unbound {};
}; // namespace PipelineSchedulingMethod

template <typename M> class PipelineScheduler {
private:
  M _;
  PipelineSchedulingTopography topography;
  std::vector<std::shared_ptr<PipelineJob>> buffer;

public:
  void schedule_job(std::shared_ptr<PipelineJob>);
  bool had_errors();
  bool was_aborted();
  void send_and_await();

  std::vector<std::shared_ptr<PipelineJob>> get_buffer();

  PipelineScheduler() = delete;
  PipelineScheduler(PipelineSchedulingTopography);
};

#endif
