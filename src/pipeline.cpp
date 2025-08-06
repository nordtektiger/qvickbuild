#include "pipeline.hpp"
#include <iostream>

std::vector<std::thread> Pipeline::thread_pool{};
std::atomic_bool Pipeline::stop_pipeline = false;
std::mutex Pipeline::queue_lock{};
std::vector<std::shared_ptr<PipelineTask>> Pipeline::task_queue{};
std::counting_semaphore<INT_MAX> Pipeline::queue_notifier{0};

void Pipeline::initialize(size_t threads) {
  for (size_t i = 0; i < threads; i++)
    thread_pool.push_back(std::thread(Pipeline::pool_loop));
}

void Pipeline::stop() {
  Pipeline::stop_pipeline = true;
  Pipeline::queue_notifier.release();
  for (std::thread &thread : Pipeline::thread_pool)
    if (thread.joinable())
      thread.join();
}

void Pipeline::push_to_queue(std::shared_ptr<PipelineTask> task_ptr) {
  std::unique_lock<std::mutex> guard(Pipeline::queue_lock);
  Pipeline::task_queue.push_back(task_ptr);
  guard.unlock();
  queue_notifier.release();
}

void Pipeline::execute_unbound(std::shared_ptr<PipelineTask> task_ptr) {
  std::thread thread{Pipeline::task_compute, task_ptr};
  thread.detach();
}

void Pipeline::pool_loop() {
  for (;;) {
    // retrieve pending task.
    Pipeline::queue_notifier.acquire();
    if (Pipeline::stop_pipeline) {
      Pipeline::queue_notifier.release(); // pass onto next thread.
      return;
    }
    std::unique_lock<std::mutex> guard(Pipeline::queue_lock);
    std::shared_ptr<PipelineTask> task = Pipeline::task_queue[0];
    Pipeline::task_queue.erase(Pipeline::task_queue.begin());
    guard.unlock();

    // execute work.
    Pipeline::task_compute(task);
  }
}

void Pipeline::task_compute(std::shared_ptr<PipelineTask> task_ptr) {
  task_ptr->compute();
  task_ptr->notifier.release();
}

void PipelineTask::await_completion() { this->notifier.acquire(); }
void PipelineTask::report_error() { this->error = true; }
bool PipelineTask::had_error() { return this->error; }

template <typename M>
void PipelineScheduler<M>::schedule_task(
    std::shared_ptr<PipelineTask> task_ptr) {
  this->buffer.push_back(task_ptr);
}

template void PipelineScheduler<PipelineSchedulingMode::SynchronousManaged>::
    schedule_task(std::shared_ptr<PipelineTask>);
template void PipelineScheduler<PipelineSchedulingMode::SynchronousUnbound>::
    schedule_task(std::shared_ptr<PipelineTask>);
template void PipelineScheduler<PipelineSchedulingMode::ParallelManaged>::
    schedule_task(std::shared_ptr<PipelineTask>);
template void PipelineScheduler<PipelineSchedulingMode::ParallelUnbound>::
    schedule_task(std::shared_ptr<PipelineTask>);

template <>
void PipelineScheduler<
    PipelineSchedulingMode::SynchronousManaged>::send_and_await() {
  for (std::shared_ptr<PipelineTask> const &task_ptr : this->buffer) {
    Pipeline::push_to_queue(task_ptr);
    task_ptr->await_completion();
  }
}

template <>
void PipelineScheduler<
    PipelineSchedulingMode::SynchronousUnbound>::send_and_await() {
  for (std::shared_ptr<PipelineTask> const &task_ptr : this->buffer) {
    Pipeline::execute_unbound(task_ptr);
    task_ptr->await_completion();
  }
}

template <>
void PipelineScheduler<
    PipelineSchedulingMode::ParallelManaged>::send_and_await() {
  for (std::shared_ptr<PipelineTask> const &task_ptr : this->buffer)
    Pipeline::push_to_queue(task_ptr);
  for (std::shared_ptr<PipelineTask> const &task_ptr : this->buffer)
    task_ptr->await_completion();
}

template <>
void PipelineScheduler<
    PipelineSchedulingMode::ParallelUnbound>::send_and_await() {
  for (std::shared_ptr<PipelineTask> const &task_ptr : this->buffer)
    Pipeline::execute_unbound(task_ptr);
  for (std::shared_ptr<PipelineTask> const &task_ptr : this->buffer)
    task_ptr->await_completion();
}
