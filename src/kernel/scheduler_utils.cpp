#include "kernel/scheduler.hpp"
#include "processes/process.hpp"
#include <sstream>
#include <iostream>
#include <syncstream>
#include <format>

// This files just contains Scheduler's Utility functions

void Scheduler::initialize_vectors() {
  this->running_ = std::vector<std::shared_ptr<Process>>(cfg_.num_cpu, nullptr);
  this->busy_ticks_per_cpu_ = std::vector<uint64_t>(cfg_.num_cpu, 0);
  this->cpu_quantum_remaining_ = std::vector<uint32_t>(cfg_.num_cpu, cfg_.quantum_cycles - 1);
}

void Scheduler::stop_barrier_sync() {
  this->tick_sync_barrier_->arrive_and_drop();
}

void Scheduler::pause() {
  std::lock_guard<std::mutex> lock(scheduler_mtx_);
  paused_.store(true);
  #if DEBUG_SCHEDULER
  std::cout <<"Paused.\n";
  #endif
}

void Scheduler::resume() {
  {
    std::lock_guard<std::mutex> lock(scheduler_mtx_);
    paused_.store(false);
  }
  pause_cv_.notify_all(); // release tick_loop wait
}

bool Scheduler::is_paused() const {
  return paused_.load();
}

uint32_t Scheduler::current_tick() const { return tick_.load(); }

std::string Scheduler::get_sched_snapshots(){
  auto snapshots = this->log_queue.snapshot();
  (void)this->log_queue.isEmpty();
  return snapshots;
}

void Scheduler::setSchedulingPolicy(SchedulingPolicy policy_){
  this->ready_queue_.setPolicy(policy_);
}

void Scheduler::tick_barrier_sync(std::string who, int barrier_index)
{
  // {
  //   const std::lock_guard<std::mutex> lock(debug_mtx_);
  //   std::osyncstream(std::cout) << "[WAIT -->] " << who
  //                 << " B#" << barrier_index
  //                 << "\n";
  // }
  this->tick_sync_barrier_->arrive_and_wait();
  // {
  //   const std::lock_guard<std::mutex> lock(debug_mtx_);
  //   std::osyncstream(std::cout) << "\e[0;30m[EXIT <--] " << who
  //             << " B#" << barrier_index
  //             << "\e[0m	\n";
  // }
};

uint32_t Scheduler::get_cpu_count() const { return cfg_.num_cpu; };

uint32_t Scheduler::get_scheduler_tick_delay() const { return cfg_.scheduler_tick_delay; }


CpuUtilization Scheduler::cpu_utilization() const {
  unsigned used = 0;
  for (const auto &runner : running_)
      if (runner) ++used;

  unsigned total = cfg_.num_cpu;
  double pct = (total > 0)
      ? (static_cast<double>(used) / total * 100.0)
      : 0.0;

  return CpuUtilization{used, total, pct};
}

void BarrierPrint::operator()() const noexcept{
  // std::osyncstream(std::cout) << std::format("=============BARRIER COMPLETE============\n");

}

size_t Scheduler::get_total_active_processes(){
  return sleep_queue_.size() + job_queue_.size() + ready_queue_.size() + running_.size();
};
