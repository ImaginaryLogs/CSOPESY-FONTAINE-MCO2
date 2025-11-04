#pragma once
#include "config.hpp"
#include "process.hpp"
#include "finished_map.hpp"
#include "cpu_worker.hpp"
#include <atomic>
#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>
#include "util.hpp"
#include <barrier>
#include <queue>
#include <functional>

// Note: May be better to implement as a Singleton as we only want one Scheduler
// Check out `docs/scheduler.md` for design notes.

using ProcessPtr = std::shared_ptr<Process>;
using ProcessCmpFn = std::function<bool(const ProcessPtr&, const ProcessPtr&)>;


struct ProcessComparator {
  bool operator()(const std::shared_ptr<Process> &a,
                  const std::shared_ptr<Process> &b) const;
};


class Scheduler {
public:
  Scheduler() = default;
  Scheduler(const Config &cfg);
  ~Scheduler();
  // === LifeCycle ===
  void start(); // spins scheduler thread
  void stop();  // stops scheduler thread

  // === Long-Term Sceduling API ===
  void submit_process(std::shared_ptr<Process> p); 

  // === Paging & Swapping (Medium-term scheduler) ===
  void handle_page_fault(std::shared_ptr<Process> p, uint64_t fault_addr);
  void swap_out_process(std::shared_ptr<Process> p);
  void swap_in_process(std::shared_ptr<Process> p);

  // === Short-Term Scheduling API ===
  std::shared_ptr<Process> dispatch_to_cpu(uint32_t cpu_id);
  void release_cpu_interrupt(uint32_t cpu_id, std::shared_ptr<Process> p, ProcessReturnContext context);
  
  // === Pre-Post Scheduling API ===
  void sleep_process(std::shared_ptr<Process> p, uint64_t duration);


  // === Diagnostics ===
  std::string snapshot(); // returns screen-ls string
  uint32_t current_tick() const;

  // === Singleton Accessor ===
  Scheduler(Scheduler &other) = delete;       // Should not be copied
  void operator=(const Scheduler &) = delete; // Should not be assigned
  Scheduler& getInstance();
  void initialize(const Config &cfg);

  void pause();
  void resume();
  bool is_paused() const;
  void tick_barrier_sync();
  void stop_barrier_sync();

  uint32_t get_cpu_count() const;
  uint32_t get_scheduler_tick_delay() const;
  std::string get_sched_snapshots();
  void setSchedulingPolicy(SchedulingPolicy policy_);
  std::string get_sleep_queue_snapshot();


private:
  // === Scheduler Internal Methods ===
  void tick_loop();
  void preemption_check();        // preemption logic
  void short_term_dispatch();     // per-CPU RR/FCFS logic
  void medium_term_check();       // page faults / swapping
  void long_term_admission();     // job -> ready
  void timer_check();
  void log_status();
  void pause_check();

  // === Internal Scheduler State === 
  Config cfg_;
  std::thread sched_thread_;
  std::atomic<uint32_t> tick_{0};
  std::atomic<bool> paused_{false};
  std::condition_variable pause_cv_;
  std::atomic<bool> sched_running_{false};
  std::mutex short_term_mtx_;
  std::mutex scheduler_mtx_;
  std::unique_ptr<std::barrier<>> tick_sync_barrier_;
  Channel<std::string> log_queue;
  std::string cpu_state_snapshot();
  
  // === Queues ===
  Channel<std::shared_ptr<Process>> job_queue_;                                           // new processes, for long-term scheduler
  DynamicVictimChannel ready_queue_;                                                      // ready process, for short-term scheduler
  Channel<std::shared_ptr<Process>> blocked_queue_;                                       // sleeping or page-faulted, medium-term scheduler
  Channel<std::shared_ptr<Process>> swapped_queue_;                                       // swapped to backing store, medium-term scheduler
  std::priority_queue<TimerEntry, std::vector<TimerEntry>, std::greater<>> sleep_queue_;  // sleep process, timer

  // === CPU State ===
  std::vector<std::shared_ptr<CPUWorker>> cpu_workers_; // cpu threads, indexed by cpu id
  std::vector<std::shared_ptr<Process>> running_;       // running processes, indexed by cpu id
  FinishedMap finished_;      // finished processes, indexed by cpu id
  
  // === Scheduler Metrics ===
  std::vector<uint64_t> busy_ticks_per_cpu_;            // Busy ticks
  std::vector<uint32_t> cpu_quantum_remaining_;         // RR bookkeeping

  // === Scheduler State ===

  // === Utilities ===
  void initialize_vectors();
  void cleanup_finished_processes(uint32_t cpu_id);
};
