#pragma once
#include "config.hpp"
#include "process.hpp"
#include <atomic>
#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>

// Note: May be better to implement as a Singleton as we only want one Scheduler
class Scheduler {
public:
  Scheduler(const Config &cfg);
  ~Scheduler();
  void start(); // spins scheduler thread
  void stop();  // stops scheduler thread
  void submit_process(std::shared_ptr<Process> p);
  std::string snapshot(); // returns screen-ls string
  // called by CPUWorker to obtain next process (scheduler assigns)
  std::shared_ptr<Process> dispatch_to_cpu(uint32_t cpu_id);
  void release_cpu(uint32_t cpu_id, std::shared_ptr<Process> p, bool finished,
                   bool yielded);
  uint32_t current_tick() const;

private:
  void tick_loop();
  Config cfg_;
  std::atomic<bool> running_{false};
  std::atomic<uint32_t> tick_{0};
  std::thread sched_thread_;
  std::deque<std::shared_ptr<Process>> ready_queue_;
  std::vector<std::shared_ptr<Process>> running_processes_; // indexed by cpu id
  std::vector<std::shared_ptr<Process>> finished_processes_;
  std::mutex mtx_;
  std::condition_variable cv_;
  // metrics
  std::vector<uint64_t> busy_ticks_per_cpu_;
  // RR bookkeeping
  std::unordered_map<uint32_t, uint32_t> cpu_quantum_remaining_;
};
