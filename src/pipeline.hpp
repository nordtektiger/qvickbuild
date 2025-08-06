#ifndef PIPELINE_HPP
#define PIPELINE_HPP

#include <atomic>
#include <memory>
#include <mutex>
#include <semaphore>
#include <thread>
#include <vector>

enum class PipelineTaskModel {
  BuildTask,
  ExecuteString,
};

class PipelineTask {
  friend class Pipeline;
  template <typename M> friend class PipelineScheduler;

private:
  std::binary_semaphore notifier;
  std::atomic_bool error;

public:
  PipelineTask() : notifier{0} {};

  virtual void compute() noexcept = 0;
  void await_completion();

  void report_error();
  bool had_error();
};

namespace PipelineTasks {
class BuildTask : public PipelineTask {
public:
  BuildTask() = default;
  void compute() noexcept;
};
class ExecuteString : public PipelineTask {
public:
  void compute() noexcept;
};
} // namespace PipelineTasks

class Pipeline {
  template <typename M> friend class PipelineScheduler;

private:
  static std::vector<std::thread> thread_pool;
  static std::atomic_bool stop_pipeline;

  static std::mutex queue_lock;
  static std::vector<std::shared_ptr<PipelineTask>> task_queue;
  static std::counting_semaphore<INT_MAX> queue_notifier;

  static void pool_loop();
  static void task_compute(std::shared_ptr<PipelineTask>);

public:
  static void push_to_queue(std::shared_ptr<PipelineTask>);
  static void execute_unbound(std::shared_ptr<PipelineTask>);
  static void initialize(size_t);
  static void stop();
};

namespace PipelineSchedulingMode {
struct SynchronousManaged {};
struct SynchronousUnbound {};
struct ParallelManaged {};
struct ParallelUnbound {};
}; // namespace PipelineSchedulingMode

template <typename M> class PipelineScheduler {
private:
  M scheduling_mode;
  std::vector<std::shared_ptr<PipelineTask>> buffer;

public:
  void schedule_task(std::shared_ptr<PipelineTask>);
  void send_and_await();
};

#endif
