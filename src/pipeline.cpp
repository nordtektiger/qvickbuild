#include "pipeline.hpp"
#include <iostream>

std::vector<std::thread> Pipeline::thread_pool{};
std::atomic_bool Pipeline::stop_pipeline = false;
std::mutex Pipeline::queue_lock{};
std::vector<std::shared_ptr<PipelineJob>> Pipeline::job_queue{};
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

void Pipeline::push_to_queue(std::shared_ptr<PipelineJob> job_ptr) {
  std::unique_lock<std::mutex> guard(Pipeline::queue_lock);
  Pipeline::job_queue.push_back(job_ptr);
  guard.unlock();
  queue_notifier.release();
}

void Pipeline::execute_unbound(std::shared_ptr<PipelineJob> job_ptr) {
  std::thread thread{Pipeline::job_compute, job_ptr};
  thread.detach();
}

void Pipeline::pool_loop() {
  for (;;) {
    // retrieve pending job.
    Pipeline::queue_notifier.acquire();
    if (Pipeline::stop_pipeline) {
      Pipeline::queue_notifier.release(); // pass onto next thread.
      return;
    }
    std::unique_lock<std::mutex> guard(Pipeline::queue_lock);
    std::shared_ptr<PipelineJob> job = Pipeline::job_queue[0];
    Pipeline::job_queue.erase(Pipeline::job_queue.begin());
    guard.unlock();

    // execute work.
    Pipeline::job_compute(job);
  }
}

void Pipeline::job_compute(std::shared_ptr<PipelineJob> job_ptr) {
  job_ptr->compute();
  job_ptr->notifier.release();
}

void PipelineJob::await_completion() { this->notifier.acquire(); }
void PipelineJob::report_error() { this->error = true; }
bool PipelineJob::had_error() { return this->error; }

template <typename M>
void PipelineScheduler<M>::schedule_job(
    std::shared_ptr<PipelineJob> job_ptr) {
  this->buffer.push_back(job_ptr);
}

template void PipelineScheduler<PipelineSchedulingMode::SynchronousManaged>::
    schedule_job(std::shared_ptr<PipelineJob>);
template void PipelineScheduler<PipelineSchedulingMode::SynchronousUnbound>::
    schedule_job(std::shared_ptr<PipelineJob>);
template void PipelineScheduler<PipelineSchedulingMode::ParallelManaged>::
    schedule_job(std::shared_ptr<PipelineJob>);
template void PipelineScheduler<PipelineSchedulingMode::ParallelUnbound>::
    schedule_job(std::shared_ptr<PipelineJob>);

template <>
void PipelineScheduler<
    PipelineSchedulingMode::SynchronousManaged>::send_and_await() {
  for (std::shared_ptr<PipelineJob> const &job_ptr : this->buffer) {
    Pipeline::push_to_queue(job_ptr);
    job_ptr->await_completion();
    if (job_ptr->had_error())
      return;
  }
}

template <>
void PipelineScheduler<
    PipelineSchedulingMode::SynchronousUnbound>::send_and_await() {
  for (std::shared_ptr<PipelineJob> const &job_ptr : this->buffer) {
    Pipeline::execute_unbound(job_ptr);
    job_ptr->await_completion();
    if (job_ptr->had_error())
      return;
  }
}

template <>
void PipelineScheduler<
    PipelineSchedulingMode::ParallelManaged>::send_and_await() {
  for (std::shared_ptr<PipelineJob> const &job_ptr : this->buffer)
    Pipeline::push_to_queue(job_ptr);
  for (std::shared_ptr<PipelineJob> const &job_ptr : this->buffer)
    job_ptr->await_completion();
}

template <>
void PipelineScheduler<
    PipelineSchedulingMode::ParallelUnbound>::send_and_await() {
  for (std::shared_ptr<PipelineJob> const &job_ptr : this->buffer)
    Pipeline::execute_unbound(job_ptr);
  for (std::shared_ptr<PipelineJob> const &job_ptr : this->buffer)
    job_ptr->await_completion();
}
