#include "../include/scheduler.hpp"
#include "../include/process.hpp"
#include "../include/cpu_worker.hpp"
#include "../src/channels.cpp"
#include "cassert"
#include <iostream>
#include <functional>
#include <thread>
#include <chrono>
#include <sstream>

#define DEBUG_SCHEDULER false

/**
 * NOTE:
 * This is just a skeleton that CJ created to get things going.
 * Feel free to add/remove/revise anything.
 */

Scheduler::Scheduler(const Config &cfg)
    : cfg_(cfg),
      busy_ticks_per_cpu_(cfg.num_cpu),
      job_queue_(Channel<std::shared_ptr<Process>>()),
      ready_queue_(cfg.scheduler),
      blocked_queue_(Channel<std::shared_ptr<Process>>()),
      swapped_queue_(Channel<std::shared_ptr<Process>>())
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

  this->tick_sync_barrier_ = std::make_unique<std::barrier<>>(
      static_cast<std::ptrdiff_t>(cfg_.num_cpu + 1));

  // Start CPU workers
  for (uint32_t i = 0; i < cfg_.num_cpu; ++i)
  {
    cpu_workers_.emplace_back(std::make_unique<CPUWorker>(i, *this));
    cpu_workers_.back()->start();
  }

  // Start scheduler tick controller
  sched_thread_ = std::thread(&Scheduler::tick_loop, this);
}

void Scheduler::stop(){
  #if DEBUG_SCHEDULER
    std::cout << "Scheduler stopping...\n";
  #endif

  sched_running_.store(false);
  paused_.store(false);
  pause_cv_.notify_all();

  for (auto &worker : cpu_workers_) worker->stop();

  if (tick_sync_barrier_) tick_sync_barrier_->arrive_and_drop();

  for (auto &worker : cpu_workers_) worker->join();

  if (sched_thread_.joinable()) sched_thread_.join();
}

// === Long-Term Scheduling API ===

void Scheduler::submit_process(std::shared_ptr<Process> p)
{
  p->set_state(ProcessState::NEW);
  this->job_queue_.send(p);
}

void Scheduler::long_term_admission()
{
  while (!this->job_queue_.isEmpty()){
    auto p = this->job_queue_.receive();
    p->set_state(ProcessState::READY);
    this->ready_queue_.send(p);
  }
}

// === Paging & Swapping (Medium-term scheduler) ===

// === Short-Term Scheduling API ===

std::shared_ptr<Process> Scheduler::dispatch_to_cpu(uint32_t cpu_id)
{
  std::lock_guard<std::mutex> lock(short_term_mtx_);

  if (this->ready_queue_.isEmpty() && !running_[cpu_id]){
    return nullptr;
  } else if (running_[cpu_id]) {
    return running_[cpu_id];
  }

  // Assign Current Process to Scheduler Internal States
  auto p = this->ready_queue_.receiveNext();

  p->set_state(ProcessState::RUNNING);
  p->cpu_id = cpu_id;
  running_[cpu_id] = p;
  p->last_active_tick = this->tick_.load();

  return p;
}

void Scheduler::release_cpu(uint32_t cpu_id, std::shared_ptr<Process> p,
                            bool finished, bool yielded)
{
  std::lock_guard<std::mutex> lock(short_term_mtx_);
  if (finished){
    p->set_state(ProcessState::FINISHED);
    running_[cpu_id] = nullptr;
    finished_[cpu_id] = p;
  } else if (yielded) {
    p->set_state(ProcessState::READY);
    ready_queue_.send(p);
    running_[cpu_id] = nullptr;
  } else {
    // Blocked or Waiting - not implemented yet
    p->set_state(ProcessState::WAITING);
    running_[cpu_id] = nullptr;
  }
}

void Scheduler::cleanup_finished_processes(uint32_t cpu_id){
  assert(finished_[cpu_id] != nullptr); // pre-condition testing
  // finished_[cpu_id] = nullptr;
  running_[cpu_id] = nullptr;
}

void Scheduler::short_term_dispatch(){ 
  for (uint32_t cpu_id = 0; cpu_id < this->cfg_.num_cpu; ++cpu_id){
    if (finished_[cpu_id]) cleanup_finished_processes(cpu_id);

    if (!running_[cpu_id]) dispatch_to_cpu(cpu_id);
  }
}

// === Pre and Post Schedulers ===

void Scheduler::preemption_check()
{
  switch (this->cfg_.scheduler)
  {
  case RR:
    #if DEBUG_SCHEDULER
      std::cout << "Preemption Check" << "\n";
    #endif
    for (uint32_t cpu_id = 0; cpu_id < this->cfg_.num_cpu; ++cpu_id){
      
      if (!running_[cpu_id]) {
        std::cout << "  CPU ID: " << cpu_id << " IDLE\n";
        continue;
      }

      #if DEBUG_SCHEDULER
      std::cout << "  CPU ID: " << cpu_id << ", PID=" << running_[cpu_id]->id() << " RR=" << cpu_quantum_remaining_[cpu_id] << " LA=" << running_[cpu_id]->last_active_tick << "\n";
      #endif

      if (cpu_quantum_remaining_[cpu_id] > 0){
        cpu_quantum_remaining_[cpu_id]--;
        continue;
      }
      if (finished_[cpu_id]) continue;

      // Time to preempt
      release_cpu(cpu_id, running_[cpu_id], false, true);
      dispatch_to_cpu(cpu_id);
      cpu_quantum_remaining_[cpu_id] = this->cfg_.quantum_cycles - 1;
    }
    break;
  case FCFS:
  case PRIORITY: // No preemption in FCFS or Priority Scheduling
    break;
  default:
    break;
  }
}

void Scheduler::timer_check(){
  while(!sleep_queue_.empty() && sleep_queue_.top().wake_tick <= this->tick_){
    auto entry = sleep_queue_.top();
    sleep_queue_.pop();
    entry.process->set_state(ProcessState::READY);
    ready_queue_.send(entry.process);
  }
}

void Scheduler::sleep_process(std::shared_ptr<Process> p, uint64_t duration){
  uint32_t wake_time = this->current_tick() + duration;
  p->set_state(ProcessState::WAITING);
  TimerEntry entry;
  entry.process = p;
  entry.wake_tick = wake_time;
  sleep_queue_.push(entry);
}

void Scheduler::log_status(){
  #if DEBUG_SCHEDULER
    std::cout << "Scheduler Tick " << this->tick_.load() << " completed.\n";
  #endif
  
  if (this->tick_ % this->cfg_.snapshot_cooldown == 0)
    log_queue.send(Scheduler::snapshot());
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

    {
      std::lock_guard<std::mutex> lock(scheduler_mtx_);

      Scheduler::preemption_check();                                              // === 1. Preemption ===
      Scheduler::tick_barrier_sync();

      if (!this->job_queue_.isEmpty())                                            // === 2. Long-term scheduling: admit new jobs ===
        Scheduler::long_term_admission();
      
      // empty for now                                                            // === 3. Middle-term scheduling: handle page faults, swapping ===

      if (!this->ready_queue_.isEmpty())                                          // === 4. Short-term scheduling: dispatch to CPUs ===
        Scheduler::short_term_dispatch();
      
      Scheduler::log_status();                                                    // === 5. Log Status ===
    }

      
    {
      std::lock_guard<std::mutex> lock(scheduler_mtx_);
      Scheduler::tick_barrier_sync();
      this->tick_.fetch_add(1);                                                   // === 5. March forward the global tick ===
      Scheduler::tick_barrier_sync();
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(Scheduler::get_scheduler_tick_delay()));
  }
}

std::string Scheduler::snapshot() {
  std::stringstream ss;
  ss << "=== Scheduler Snapshot ===\n";
  ss << "Tick: " << tick_.load() << "\n";
  ss << "Paused: " << (paused_.load() ? "true" : "false") << "\n";
  // --- Job Queue ---
  ss << "[Job Queue]\n";
  if (job_queue_.isEmpty())
  {
    ss << "  (empty)\n";
  }
  else
  {
    ss << job_queue_.snapshot();
  }

  // --- Ready Queue ---
  ss << "\n[Ready Queue]\n";
  if (ready_queue_.isEmpty())
  {
    ss << "  (empty)\n";
  }
  else
  {
    ss << ready_queue_.snapshot();
  }

  // --- CPU States ---
  ss << "\n[CPU States]\n";
  for (size_t i = 0; i < running_.size(); ++i)
  {
    auto &proc = running_[i];
    if (proc)
      ss << "  CPU " << i 
         << ": PID=" << proc->id()
         << "  RR=" << cpu_quantum_remaining_[i]
         << " LA=" << proc->last_active_tick
         << " (" << proc->get_state_string() << ")\n";
    else
      ss << "  CPU " << i << ": IDLE\n";
  }

  // --- Finished ---
  ss << "\n[Finished Processes]\n";
  bool any_finished = false;
  for (auto &p : finished_)
  {
    if (p)
    {
      ss << "  PID=" << p->id()
         << " (" << p->get_state_string() << ")\n";
      any_finished = true;
    }
  }
  if (!any_finished)
    ss << "  (none)\n";

  ss << "===========================\n";

  return ss.str();
}
