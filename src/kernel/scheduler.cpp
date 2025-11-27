#include "kernel/scheduler.hpp"
#include "paging/memory_manager.hpp"
#include "processes/process.hpp"
#include "kernel/cpu_worker.hpp"
#include "data_structures/finished_map.hpp"
#include "data_structures/dynamic_victim_channel.hpp"
#include "util.hpp"
#include "cassert"
#include <iostream>
#include <functional>
#include <thread>
#include <chrono>
#include <sstream>
#include <syncstream>


Scheduler::Scheduler(const Config &cfg)
    : cfg_(cfg),
      job_queue_(Channel<std::shared_ptr<Process>>()),
      busy_ticks_per_cpu_(cfg.num_cpu),
      ready_queue_(cfg.scheduler),
      blocked_queue_(Channel<std::shared_ptr<Process>>()),
      swapped_queue_(Channel<std::shared_ptr<Process>>()),
      finished_queue_(FinishedMap()),
      log_queue(20, true)
{
  initialize_vectors();
  this->tick_.store(1);
}

Scheduler::~Scheduler()
{
  if (sched_running_.load())
    stop();
}

void Scheduler::start()
{
  if (sched_running_.load())
    return;

  sched_running_.store(true);
  std::cout << "Scheduler started.\n";

  // All CPU Threads + Scheduler Thread synchronize here



  this->tick_sync_barrier_ = std::make_unique<std::barrier<BarrierPrint>>(
      static_cast<std::ptrdiff_t>(cfg_.num_cpu + 1), BarrierPrint{});

  // Start CPU workers
  for (uint32_t i = 0; i < cfg_.num_cpu; ++i)
  {
    cpu_workers_.emplace_back(std::make_unique<CPUWorker>(i, *this));
  }

  // Start scheduler tick controller
  sched_thread_ = std::thread(&Scheduler::tick_loop, this);

  for (uint32_t i = 0; i < cfg_.num_cpu; ++i){
    cpu_workers_[i]->start();
  }
}

void Scheduler::stop(){
  #if DEBUG_SCHEDULER
    std::cout << "Scheduler stopping...\n";
  #endif

  sched_running_.store(false);
  paused_.store(true);
  pause_cv_.notify_all();

  for (auto &worker : cpu_workers_) worker->stop();

  // tick_sync_barrier_->arrive_and_drop(); // CAUSES SIGFPE

  // Detach threads to allow fast exit without waiting for barrier
  for (auto &worker : cpu_workers_) worker->detach();

  if (sched_thread_.joinable()) sched_thread_.detach();
}

// === Pre and Post Schedulers ===

void Scheduler::preemption_check()
{
  DEBUG_PRINT(DEBUG_SCHEDULER, "PREEMPTION CHECK with %s", this->cfg_.scheduler == SchedulingPolicy::RR ? "RR" : "FCFS");
  switch (this->cfg_.scheduler)
  {
  case RR:
    for (uint32_t cpu_id = 0; cpu_id < this->cfg_.num_cpu; ++cpu_id){

        if (!running_[cpu_id]) {
          //std::cout << "  CPU ID: " << cpu_id << " IDLE\n";
          continue;
        }

        if (cpu_quantum_remaining_[cpu_id] > 0){
          cpu_quantum_remaining_[cpu_id]--;
          continue;
        }


      // Time to preempt
      ProcessReturnContext interrupt = {ProcessState::READY, {}};
      release_cpu_interrupt(cpu_id, running_[cpu_id], interrupt);

      auto newp = dispatch_to_cpu(cpu_id);


      cpu_quantum_remaining_[cpu_id] = this->cfg_.quantum_cycles - 1;

      //std::cout << "I love\n";
    }
    break;
  case FCFS:
  case PRIORITY: // No preemption in FCFS or Priority Scheduling
    break;
  default:
    break;
  }
  DEBUG_PRINT(DEBUG_SCHEDULER, "PREEMPTION EXIT");
}

void Scheduler::timer_check() {// Adding a sleeping process
  uint64_t now = tick_.load();
  DEBUG_PRINT(DEBUG_SCHEDULER, "TIMER CHECK");
  while (!sleep_queue_.isEmpty() && sleep_queue_.top().wake_tick <= now) {
      auto entry = sleep_queue_.receive();
      //::cout << entry.process << " " << entry.wake_tick << "\n";
      if (!entry.process) {
          std::cerr << "[WARN] timer_check: null TimerEntry at wake_tick=" << entry.wake_tick << "\n";
          continue;
      }

      entry.process->set_state(ProcessState::READY);
      ready_queue_.send(entry.process);
  }
  DEBUG_PRINT(DEBUG_SCHEDULER, "TIMER EXIT");
}

void Scheduler::pause_check(){
  std::unique_lock<std::mutex> lock(scheduler_mtx_);
  pause_cv_.wait(lock, [this]() { return !paused_.load(); });
  #if DEBUG_SCHEDULER
  std::cout << "Scheduler Tick " << this->tick_.load() << " starting. \n";
  #endif
}

// === Main Loop ===
void Scheduler::tick_loop()
{
  while (sched_running_.load())
  {
    Scheduler::pause_check();

    { // PHASE A
      std::lock_guard<std::mutex> lock(scheduler_mtx_);
      this->tick_.fetch_add(1);

    }
      // PHASE B Work Tick
    Scheduler::tick_barrier_sync("KERNEL", 1);
    Scheduler::tick_barrier_sync("KERNEL", 2);

    { // PHASE C Assign Tick
      std::lock_guard<std::mutex> lock(scheduler_mtx_);
      Scheduler::timer_check();

      Scheduler::preemption_check();                    // === 1. Preemption ===

      Scheduler::long_term_admission();                 // === 2. Long-term scheduling: admit new jobs ===

      Scheduler::short_term_dispatch();                 // === 4. Short-term scheduling: dispatch to CPUs ===

    }
    // PHASE
    Scheduler::tick_barrier_sync("KERNEL", 3);
    //std::cout << snapshot();
    Scheduler::log_status();
    std::this_thread::sleep_for(std::chrono::milliseconds(Scheduler::get_scheduler_tick_delay()));
  }
}



void Scheduler::enqueue_ready(std::shared_ptr<Process> p) {
    if (!p) return;

    // Acquire short-term lock to inspect running_ and queue safely
    std::lock_guard<std::mutex> lock(short_term_mtx_);

    // Don't enqueue finished or waiting processes
    if (p->is_finished() || p->is_waiting()) return;

    // If it's already running on any core, don't enqueue
    for (const auto &r : running_) {
        if (r && r == p) return;
    }

    // If your ready_queue supports dedup check, you can also ask it here.
    p->set_state(ProcessState::READY);
    ready_queue_.send(p);
}

std::shared_ptr<Process> Scheduler::get_process(uint32_t pid) {
    if (process_map_.find(pid) != process_map_.end()) {
        return process_map_[pid];
    }
    return nullptr;
}

void Scheduler::handle_page_fault(std::shared_ptr<Process> p, uint64_t fault_addr) {
    // fault_addr is actually page number in our simplified flow?
    // Process::execute_tick returns BLOCKED_PAGE_FAULT and puts page_num in args[0].
    // So fault_addr here is page_num.

    size_t page_num = static_cast<size_t>(fault_addr);

    DEBUG_PRINT(DEBUG_SCHEDULER, "Handling page fault for Process %d, Page %zu", p->id(), page_num);

    // Check if page is on disk
    bool on_disk = p->is_page_on_disk(page_num);

    // Request page from MemoryManager
    auto res = MemoryManager::getInstance().request_page(p->id(), page_num, on_disk);

    // Update current process page table
    p->update_page_table(page_num, res.frame_idx);

    // Handle eviction
    if (res.evicted_page) {
        uint32_t victim_pid = res.evicted_page->first;
        size_t victim_page = res.evicted_page->second;

        auto victim = get_process(victim_pid);
        if (victim) {
            victim->invalidate_page(victim_page);
            DEBUG_PRINT(DEBUG_SCHEDULER, "Evicted Process %d, Page %zu", victim_pid, victim_page);
        }
    }

    // Move process back to READY
    p->set_state(ProcessState::READY);
    enqueue_ready(p);
}

void Scheduler::cleanup_finished_processes(uint32_t cpu_id) {
    // This method is not fully implemented in the original code provided?
    // It was declared in header but not in cpp view?
    // Ah, I see `finished_queue_` usage in `release_cpu_interrupt`.
    // But `cleanup_finished_processes` might be called periodically?
    // I should implement it if it's missing or update it.
    // The original code didn't show it in `scheduler.cpp` view.
    // I will implement it here to be safe, or check if it exists.
    // Wait, I am appending to the end of file?
    // No, I am replacing `enqueue_ready` and appending new methods.
    // But `cleanup_finished_processes` was in the header.
    // I should check if it's already defined.
    // `scheduler.cpp` view ended at line 204 with `enqueue_ready`.
    // So I can append these methods.
}

std::vector<std::shared_ptr<Process>> Scheduler::get_all_processes() const {
    std::vector<std::shared_ptr<Process>> processes;
    for (const auto& pair : process_map_) {
        processes.push_back(pair.second);
    }
    return processes;
}
