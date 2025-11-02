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

#define DEBUG_SCHEDULER true

Scheduler::Scheduler(const Config &cfg)
    : cfg_(cfg),
      busy_ticks_per_cpu_(cfg.num_cpu),
      job_queue_(Channel<std::shared_ptr<Process>>()),
      ready_queue_(SchedulingPolicy::FCFS),
      blocked_queue_(Channel<std::shared_ptr<Process>>()),
      swapped_queue_(Channel<std::shared_ptr<Process>>())
{
  initialize_vectors();
  this->tick_.store(0);
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

void Scheduler::stop()
{
#if DEBUG_SCHEDULER
  std::cout << "Scheduler stopping...\n";
#endif

  sched_running_.store(false);
  paused_.store(false);
  pause_cv_.notify_all();
  for (auto &worker : cpu_workers_)
  {
    worker->stop();
  }
  if (tick_sync_barrier_)
    tick_sync_barrier_->arrive_and_drop();

  for (auto &worker : cpu_workers_)
  {
    
    worker->join();
  }

  if (sched_thread_.joinable())
  {
    sched_thread_.join();
  }
}

// === Long-Term Scheduling API ===

void Scheduler::submit_process(std::shared_ptr<Process> p)
{
  p->set_state(ProcessState::NEW);
  this->job_queue_.send(p);
}

void Scheduler::long_term_admission()
{
  auto p = this->job_queue_.receive();
  p->set_state(ProcessState::READY);
  this->ready_queue_.send(p);
}

// === Paging & Swapping (Medium-term scheduler) ===

// === Short-Term Scheduling API ===

std::shared_ptr<Process> Scheduler::dispatch_to_cpu(uint32_t cpu_id)
{
  std::lock_guard<std::mutex> lock(short_term_mtx_);
  if (this->ready_queue_.isEmpty())
  {
    return nullptr;
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
  if (finished)
  {
    p->set_state(ProcessState::FINISHED);
    finished_[cpu_id] = p;
  }
  else if (yielded)
  {
    p->set_state(ProcessState::READY);
    ready_queue_.send(p);
    running_[cpu_id] = nullptr;
  }
  else
  {
    // Blocked or Waiting - not implemented yet
    p->set_state(ProcessState::WAITING);
    running_[cpu_id] = nullptr;
  }
}

void Scheduler::tick_barrier_sync()
{
  this->tick_sync_barrier_->arrive_and_wait();
}

uint32_t Scheduler::get_cpu_count() const { return cfg_.num_cpu; };
uint32_t Scheduler::get_scheduler_tick_delay() const { return cfg_.scheduler_tick_delay; }

void Scheduler::cleanup_finished_processes(uint32_t cpu_id)
{
  assert(finished_[cpu_id] != nullptr); // pre-condition testing
  finished_[cpu_id] = nullptr;
  running_[cpu_id] = nullptr;
}

void Scheduler::short_term_dispatch()
{
  for (uint32_t cpu_id = 0; cpu_id < this->cfg_.num_cpu; ++cpu_id)
  {
    if (finished_[cpu_id])
    {
      finished_[cpu_id] = nullptr;
      running_[cpu_id] = nullptr;
    }
    if (!running_[cpu_id])
      dispatch_to_cpu(cpu_id);
  }
}

void Scheduler::preemption_check()
{
  switch (this->cfg_.scheduler)
  {
  case RR:
    for (uint32_t cpu_id = 0; cpu_id < this->cfg_.num_cpu; ++cpu_id)
    {
      if (!running_[cpu_id])
        continue;

      if (cpu_quantum_remaining_[cpu_id] > 0)
      {
        cpu_quantum_remaining_[cpu_id]--;
        continue;
      }

      // Time to preempt
      release_cpu(cpu_id, running_[cpu_id], false, true);
      dispatch_to_cpu(cpu_id);
      cpu_quantum_remaining_[cpu_id] = this->cfg_.quantum_cycles;
    }
    break;
  case FCFS:
  case PRIORITY:
    // No preemption in FCFS or Priority Scheduling
    break;
  default:
    break;
  }
}

// === Main Loop ===
void Scheduler::tick_loop()
{
  while (sched_running_.load())
  {

    {
      std::unique_lock<std::mutex> lock(scheduler_mtx_);
      pause_cv_.wait(lock, [this]()
                     { return !paused_.load(); });
    }
#if DEBUG_SCHEDULER
    std::cout << "Scheduler Tick " << this->tick_.load() << " starting. \n";
#endif

    {
      std::lock_guard<std::mutex> lock(scheduler_mtx_);
      // === 1. Preemption ===
      Scheduler::preemption_check();

      // === 2. Long-term scheduling: admit new jobs ===
      if (!this->job_queue_.isEmpty())
      {
        Scheduler::long_term_admission();
      }

      // === 3. Middle-term scheduling: handle page faults, swapping ===
      // empty for now

      // === 4. Short-term scheduling: dispatch to CPUs ===
      if (!this->ready_queue_.isEmpty())
      {
        Scheduler::short_term_dispatch();
      }

#if DEBUG_SCHEDULER
      std::cout << "Scheduler Tick " << this->tick_.load() << " completed.\n";
#endif

      Scheduler::tick_barrier_sync();

      // === 5. March forward the global tick ===
      this->tick_.fetch_add(1);

#if DEBUG_SCHEDULER
      std::cout << "Scheduler Tick advanced to " << this->tick_.load() << ".\n";
#endif
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(Scheduler::get_scheduler_tick_delay()));
  }
}

std::string Scheduler::snapshot()
{
  std::stringstream ss;
  std::lock_guard<std::mutex> lock(scheduler_mtx_);
  ss << "=== Scheduler Snapshot ===\n";
  ss << "Tick: " << tick_.load() << "\n";
  ss << "Paused: " << (paused_.load() ? "true" : "false") << "\n\n";

  // --- CPU States ---
  ss << "[CPU States]\n";
  for (size_t i = 0; i < running_.size(); ++i)
  {
    auto &proc = running_[i];
    if (proc)
      ss << "  CPU " << i << ": PID=" << proc->id()
         << " (" << proc->get_state_string() << ")\n";
    else
      ss << "  CPU " << i << ": IDLE\n";
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

void Scheduler::pause()
{
  std::lock_guard<std::mutex> lock(scheduler_mtx_);
  paused_.store(true);
}

void Scheduler::resume()
{
  {
    std::lock_guard<std::mutex> lock(scheduler_mtx_);
    paused_.store(false);
  }
  pause_cv_.notify_all(); // release tick_loop wait
}

bool Scheduler::is_paused() const
{
  return paused_.load();
}

uint32_t Scheduler::current_tick() const { return tick_.load(); }
