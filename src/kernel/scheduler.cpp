#include "kernel/scheduler.hpp"
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

  if (tick_sync_barrier_) tick_sync_barrier_->arrive_and_drop();

  for (auto &worker : cpu_workers_) worker->join();

  if (sched_thread_.joinable()) sched_thread_.join();
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