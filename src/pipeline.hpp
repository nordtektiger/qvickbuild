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
  friend class PipelineScheduler;

private:
  std::binary_semaphore notifier;

public:
  PipelineTask() : notifier{0} {};

  virtual void compute() = 0;
  void await_completion();
};

namespace PipelineTasks {
class BuildTask : public PipelineTask {
public:
  BuildTask() = default;
  void compute() {}
};
class ExecuteString : public PipelineTask {
public:
};
} // namespace PipelineTasks

class Pipeline {
  friend class PipelineScheduler;

private:
  static std::vector<std::thread> thread_pool;
  static std::atomic_bool stop_pipeline;

  static std::mutex queue_lock;
  static std::vector<std::shared_ptr<PipelineTask>> task_queue;
  static std::counting_semaphore<INT_MAX> queue_notifier;

  static void pipeline_worker();

public:
  static void push_to_queue(std::shared_ptr<PipelineTask>);
  static void initialize(size_t);
  static void stop();
};

enum class PipelineSchedulingMode {
  Synchronous,
  Parallel,
};

class PipelineScheduler {
private:
  PipelineSchedulingMode scheduling_mode;

public:
  void schedule_task(std::shared_ptr<PipelineTask> task_ptr);
  void send_and_await();
};

#endif
