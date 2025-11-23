#include "../include/scheduler.hpp"
#include "../include/process.hpp"
#include "../include/cpu_worker.hpp"
#include "../include/finished_map.hpp"
#include "../include/util.hpp"
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
      
      job_queue_(Channel<std::shared_ptr<Process>>()),
      busy_ticks_per_cpu_(cfg.num_cpu),
      ready_queue_(cfg.scheduler),
      blocked_queue_(Channel<std::shared_ptr<Process>>()),
      swapped_queue_(Channel<std::shared_ptr<Process>>()),
      finished_(FinishedMap())
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
    std::lock_guard<std::mutex> short_lock(short_term_mtx_);

    // If CPU already running a process, return it
    if (running_[cpu_id]) {
        if (!running_[cpu_id].get()) {
            std::cerr << "[WARN] running_[" << cpu_id << "] is null!\n";
            running_[cpu_id].reset();
            return nullptr;
        }
        return running_[cpu_id];
    }

    // Nothing to schedule
    if (this->ready_queue_.isEmpty()) {
        return nullptr;
    }

    // Fetch next process
    auto p = this->ready_queue_.receiveNext();
    if (!p) {
        //std::cerr << "[ERROR] Scheduler::dispatch_to_cpu received null process from ready_queue_\n";
        return nullptr;
    }

    for (const auto &r : running_) {
        if (r && r == p) {
            return nullptr;
        }
    }

    // Assign to CPU
    p->set_state(ProcessState::RUNNING);
    p->cpu_id = cpu_id;
    p->last_active_tick = this->tick_.load();
    running_[cpu_id] = p;

    if (cpu_quantum_remaining_.size() > cpu_id)
        cpu_quantum_remaining_[cpu_id] = this->cfg_.quantum_cycles - 1;
    return p;
}

void Scheduler::release_cpu_interrupt(uint32_t cpu_id, std::shared_ptr<Process> p, ProcessReturnContext context)
{ 
  std::lock_guard<std::mutex> lock(short_term_mtx_);  

  if (!p) {
    std::cerr << "[ERROR] release_cpu_interrupt called with null process (cpu " << cpu_id << ")\n";
    return;
  }

  
  if (p->is_finished() || context.state == ProcessState::FINISHED){
    p->set_state(ProcessState::FINISHED);
    running_[cpu_id] = nullptr;
    finished_.insert(p, tick_ + 1);
    return;
  } else if (p->is_waiting() || context.state == ProcessState::WAITING) {
    p->set_state(ProcessState::WAITING);
    running_[cpu_id] = nullptr;

    uint64_t duration = 0;
    try {
        if (!context.args.empty()) duration = std::stoull(context.args.at(0));
    } catch (...) { duration = 0; }

    uint64_t now = tick_.load();
    uint64_t wake_time = now + duration;
    sleep_queue_.send(p, wake_time);
    return;
  } else if (context.state == ProcessState::READY) {
        // Preemption: move back to ready queue
        p->set_state(ProcessState::READY);
        running_[cpu_id] = nullptr;

        // Use enqueue_ready to avoid duplicates
        enqueue_ready(p);
        return;
  } else if (context.state == ProcessState::RUNNING) {
    return;
  }

  p->set_state(ProcessState::READY);
  running_[cpu_id] == nullptr;
  enqueue_ready(p);
}




void Scheduler::short_term_dispatch(){ 
  for (uint32_t cpu_id = 0; cpu_id < this->cfg_.num_cpu; ++cpu_id){

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
      {
      std::lock_guard<std::mutex> lock(short_term_mtx_);

      if (!running_[cpu_id]) {
        //std::cout << "  CPU ID: " << cpu_id << " IDLE\n";
        continue;
      }

      if (cpu_quantum_remaining_[cpu_id] > 0){
        cpu_quantum_remaining_[cpu_id]--;
        continue;
      }
    }
      
      // Time to preempt
      ProcessReturnContext interrupt = {ProcessState::READY, {}};
      release_cpu_interrupt(cpu_id, running_[cpu_id], interrupt);

      auto newp = dispatch_to_cpu(cpu_id);
      if (newp){ 
        std::lock_guard<std::mutex> lock(short_term_mtx_);
        cpu_quantum_remaining_[cpu_id] = this->cfg_.quantum_cycles - 1;
      }
      std::cout << "I love\n";
    }
    break;
  case FCFS:
  case PRIORITY: // No preemption in FCFS or Priority Scheduling
    break;
  default:
    break;
  }
}

void Scheduler::timer_check() {// Adding a sleeping process
    uint64_t now = tick_.load();
    while (!sleep_queue_.isEmpty() && sleep_queue_.top().wake_tick <= now) {
        auto entry = sleep_queue_.receive();

        if (!entry.process) {
            std::cerr << "[WARN] timer_check: null TimerEntry at wake_tick=" << entry.wake_tick << "\n";
            continue;
        }

        entry.process->set_state(ProcessState::READY);
        ready_queue_.send(entry.process);
    }
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
      Scheduler::timer_check();
      Scheduler::preemption_check();                                              // === 1. Preemption ===
      Scheduler::tick_barrier_sync();

      if (!this->job_queue_.isEmpty())                                            // === 2. Long-term scheduling: admit new jobs ===
        Scheduler::long_term_admission();
      
      // empty for now                                                            // === 3. Middle-term scheduling: handle page faults, swapping ===


      if (!this->ready_queue_.isEmpty())                                          // === 4. Short-term scheduling: dispatch to CPUs ===
        Scheduler::short_term_dispatch();
      
      Scheduler::log_status();                                                    // === 5. Log Status ===

      Scheduler::tick_barrier_sync();
      this->tick_.fetch_add(1);                                                   // === 6. March forward the global tick ===
      Scheduler::tick_barrier_sync();
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(Scheduler::get_scheduler_tick_delay()));
  }
}
std::string Scheduler::cpu_state_snapshot() {
    std::vector<std::shared_ptr<Process>> procs;
    std::vector<uint32_t> quanta;
    
    procs = running_;
    quanta = cpu_quantum_remaining_;
     // unlock here before printing

    std::ostringstream oss;
    auto t = std::time(nullptr);
    auto tm = *std::localtime(&t);

    for (size_t i = 0; i < procs.size(); ++i) {
        auto &proc = procs[i];
        if (proc) {
            oss << proc->name() << "\t"
                << std::put_time(&tm, "%d-%m-%Y %H-%M-%S") << "\t"
                << "PID=" << proc->id() << "\t"
                << "RR=" << quanta[i] << "\t"
                << "LA=" << proc->last_active_tick << "\t"
                << "Core: " << i << "\t"
                << proc->get_executed_instructions() << " / "
                << proc->get_total_instructions() << "\n";
        } else {
            oss << "  CPU " << i << ": IDLE\n";
        }
    }
    return oss.str();
}


std::string Scheduler::snapshot() {
  std::ostringstream oss;
  oss << "=== Scheduler Snapshot ===| ---\n";
  oss << "Tick: " << tick_.load() << "\n";
  oss << "Paused: " << (paused_.load() ? "true" : "false") << "\n";

  oss << "[Sleep Queue]\n"
      << (((sleep_queue_.isEmpty()))
        ? " (empty)\n"
        : sleep_queue_.print_sleep_queue());
  
  // --- Job Queue ---
  oss << "[Job Queue]\n"
      << ((job_queue_.isEmpty())
        ? " (empty)\n" 
        : job_queue_.snapshot());

  // --- Ready Queue ---
  oss << "[Ready Queue]\n";
  oss << (ready_queue_.isEmpty() 
        ? "  (empty)\n" 
        : ready_queue_.snapshot());
  
  // --- CPU States ---
  oss << "\n[CPU States]:\n";
  oss << cpu_state_snapshot();  

  // --- Finished ---
  oss << "[Finished Processes]:\n";
  std::string finished_snapshot = finished_.snapshot();
  oss << ((finished_snapshot.empty()) 
        ? " (none)\n"
        : finished_snapshot);

  oss << "===========================\n";

  return oss.str();
}


std::vector<TimerEntry> TimerEntrySleepQueue::get_sleep_queue_snapshot() const {
    std::lock_guard<std::mutex> lock(sleep_queue_mtx_);

    // Make a shallow copy of the priority_queue’s contents safely
    auto copy = sleep_queue_;
    std::vector<TimerEntry> snapshot;
    while (!copy.empty()) {
        snapshot.push_back(copy.top());
        copy.pop();
    }
    return snapshot;
}

std::string TimerEntrySleepQueue::print_sleep_queue() const {
    std::lock_guard<std::mutex> lock(sleep_queue_mtx_);
    std::priority_queue<TimerEntry> copy = sleep_queue_;
    std::ostringstream oss;
    while (!copy.empty()) {
        const auto &t = copy.top();
        if (t.process)
            oss << t.process->name()
                << "\tPID:" << t.process->id() << "\tWT " << t.wake_tick << "\n";
        else
            oss << "  [NULL process] wakes at " << t.wake_tick << "\n";
        copy.pop();
    }
    return oss.str();
}

void TimerEntrySleepQueue::send(std::shared_ptr<Process> p, uint64_t wake_tick) {
    if (!p) {
        std::cerr << "[ERROR] Tried to queue null process for sleep\n";
        return;
    }
    // Adding a sleeping process
    std::lock_guard<std::mutex> lock(sleep_queue_mtx_);
    TimerEntry t{p, wake_tick};
    sleep_queue_.push(t);
}

TimerEntry TimerEntrySleepQueue::receive() {
    std::lock_guard<std::mutex> lock(sleep_queue_mtx_);
    if (sleep_queue_.empty()) {
        return TimerEntry{nullptr, 0};
    }
    TimerEntry t = sleep_queue_.top();
    sleep_queue_.pop();
    return t;
}

bool TimerEntrySleepQueue::isEmpty() const {
    std::lock_guard<std::mutex> lock(sleep_queue_mtx_);
    return sleep_queue_.empty();
}

TimerEntry TimerEntrySleepQueue::top() const {
    std::lock_guard<std::mutex> lock(sleep_queue_mtx_);
    if (sleep_queue_.empty()) {
        return TimerEntry{nullptr, 0};
    }
    return sleep_queue_.top();
}


std::string Scheduler::get_sleep_queue_snapshot() {
    std::lock_guard<std::mutex> lock(scheduler_mtx_);
    return sleep_queue_.print_sleep_queue();
}

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