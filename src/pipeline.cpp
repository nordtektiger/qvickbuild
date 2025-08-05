#include "pipeline.hpp"
#include <iostream>

std::vector<std::thread> Pipeline::thread_pool{};
std::atomic_bool Pipeline::stop_pipeline = false;
std::mutex Pipeline::queue_lock{};
std::vector<std::shared_ptr<PipelineTask>> Pipeline::task_queue{};
std::counting_semaphore<INT_MAX> Pipeline::queue_notifier{0};

void Pipeline::initialize(size_t threads) {
  for (size_t i = 0; i < threads; i++)
    thread_pool.push_back(std::thread(Pipeline::pipeline_worker));
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

void Pipeline::pipeline_worker() {
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
    task->compute();

    // notify waiting thread.
    task->notifier.release();
  }
}

void PipelineTask::await_completion() { this->notifier.acquire(); }
