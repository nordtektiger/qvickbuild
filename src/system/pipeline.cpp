#include "pipeline.hpp"
#include <cassert>
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

void Pipeline::stop_sync() {
  Pipeline::stop_pipeline = true;
  Pipeline::queue_notifier.release();
  for (std::thread &thread : Pipeline::thread_pool)
    if (thread.joinable())
      thread.join();
}

void Pipeline::stop_async() {
  Pipeline::stop_pipeline = true;
  Pipeline::queue_notifier.release();
}

void Pipeline::abort_queued() {
  for (std::shared_ptr<PipelineJob> &job : Pipeline::job_queue) {
    job->mark_aborted();
    job->notifier.release(); // allow waiting client to return.
  }
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
    if (!job->was_aborted())
      Pipeline::job_compute(job);

    if (job->had_error())
      Pipeline::abort_queued();
  }
}

void Pipeline::job_compute(std::shared_ptr<PipelineJob> job_ptr) {
  job_ptr->compute();
  job_ptr->notifier.release();
}

void PipelineJob::await_completion() { this->notifier.acquire(); }
void PipelineJob::report_error() { this->error = true; }
bool PipelineJob::had_error() const { return this->error; }
void PipelineJob::mark_aborted() { this->aborted = true; }
bool PipelineJob::was_aborted() const { return this->aborted; }

template <typename M>
PipelineScheduler<M>::PipelineScheduler(
    PipelineSchedulingTopography topograhy) {
  this->topography = topograhy;
}

template PipelineScheduler<PipelineSchedulingMethod::Managed>::
    PipelineScheduler(PipelineSchedulingTopography);
template PipelineScheduler<PipelineSchedulingMethod::Unbound>::
    PipelineScheduler(PipelineSchedulingTopography);

template <typename M>
void PipelineScheduler<M>::schedule_job(std::shared_ptr<PipelineJob> job_ptr) {
  this->buffer.push_back(job_ptr);
}

template void
    PipelineScheduler<PipelineSchedulingMethod::Managed>::schedule_job(
        std::shared_ptr<PipelineJob>);
template void
    PipelineScheduler<PipelineSchedulingMethod::Unbound>::schedule_job(
        std::shared_ptr<PipelineJob>);

template <typename M> bool PipelineScheduler<M>::had_errors() {
  for (std::shared_ptr<PipelineJob> const &job_ptr : this->buffer) {
    if (job_ptr->had_error())
      return true;
  }
  return false;
}

template bool
PipelineScheduler<PipelineSchedulingMethod::Managed>::had_errors();
template bool
PipelineScheduler<PipelineSchedulingMethod::Unbound>::had_errors();

template <typename M> bool PipelineScheduler<M>::was_aborted() {
  for (std::shared_ptr<PipelineJob> const &job_ptr : this->buffer) {
    if (job_ptr->was_aborted())
      return true;
  }
  return false;
}

template bool
PipelineScheduler<PipelineSchedulingMethod::Managed>::was_aborted();
template bool
PipelineScheduler<PipelineSchedulingMethod::Unbound>::was_aborted();

template <>
void PipelineScheduler<PipelineSchedulingMethod::Managed>::send_and_await() {
  if (topography == PipelineSchedulingTopography::Sequential) {
    for (std::shared_ptr<PipelineJob> const &job_ptr : this->buffer) {
      Pipeline::push_to_queue(job_ptr);
      job_ptr->await_completion();
      if (job_ptr->had_error())
        return;
    }
  } else if (topography == PipelineSchedulingTopography::Parallel) {
    for (std::shared_ptr<PipelineJob> const &job_ptr : this->buffer)
      Pipeline::push_to_queue(job_ptr);
    for (std::shared_ptr<PipelineJob> const &job_ptr : this->buffer)
      job_ptr->await_completion();
  } else {
    assert(false && "invalid pipeline scheduling topography");
  }
}

template <>
void PipelineScheduler<PipelineSchedulingMethod::Unbound>::send_and_await() {
  if (topography == PipelineSchedulingTopography::Sequential) {
    for (std::shared_ptr<PipelineJob> const &job_ptr : this->buffer) {
      Pipeline::execute_unbound(job_ptr);
      job_ptr->await_completion();
      if (job_ptr->had_error())
        return;
    }
  } else if (topography == PipelineSchedulingTopography::Parallel) {
    for (std::shared_ptr<PipelineJob> const &job_ptr : this->buffer)
      Pipeline::execute_unbound(job_ptr);
    for (std::shared_ptr<PipelineJob> const &job_ptr : this->buffer)
      job_ptr->await_completion();
  } else {
    assert(false && "invalid pipeline scheduling topography");
  }
}
