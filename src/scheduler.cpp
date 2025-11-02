#include "../include/scheduler.hpp"
#include "cassert"

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
  this->busy_ticks_per_cpu_ = std::vector<uint64_t>();
  this->cpu_quantum_remaining_ = std::unordered_map<uint32_t, uint32_t>();

  for (uint32_t i = 0; i < cfg_.num_cpu; i++) {
    busy_ticks_per_cpu_[i] = 0;
    cpu_quantum_remaining_[i] = 0;
    
  }
  
  sched_running_.store(true);
  sched_thread_ = std::thread(&Scheduler::tick_loop, this);
}

Scheduler::~Scheduler() {
  if (sched_running_.load()) {
    stop();
  }
}

void Scheduler::start() {
  if (sched_running_.load())
    return;

  
}

void Scheduler::stop() {
  sched_running_.store(false);
  if (sched_thread_.joinable()) {
    sched_thread_.join();
  }
}

// === Long-Term Scheduling API ===

void Scheduler::submit_process(std::shared_ptr<Process> p) {
  p->set_state(ProcessState::NEW);
  this->job_queue_.send(p);
}

void Scheduler::long_term_admission() {
  auto p = this->job_queue_.receive();
  p->set_state(ProcessState::READY);
  this->ready_queue_.send(p);
}

// === Paging & Swapping (Medium-term scheduler) ===

// === Short-Term Scheduling API ===

std::shared_ptr<Process> Scheduler::dispatch_to_cpu(uint32_t cpu_id) {
  std::lock_guard<std::mutex> lock(shortTermMtx_);
  if (this->ready_queue_.isEmpty()) {
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
                            bool finished, bool yielded) {
  std::lock_guard<std::mutex> lock(shortTermMtx_);
  if (finished) {
    p->set_state(ProcessState::FINISHED);
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

void Scheduler::cleanup_finished_processes(uint32_t cpu_id) {
  assert(finished_[cpu_id] != nullptr); // pre-condition testing

  finished_[cpu_id] = nullptr;
  running_[cpu_id] = nullptr;
}



void Scheduler::short_term_dispatch(){
  for (uint32_t cpu_id = 0; cpu_id < this->cfg_.num_cpu; ++cpu_id) {
    if (finished_[cpu_id]) {
      finished_[cpu_id] = nullptr;
      running_[cpu_id] = nullptr;
    }
    
    if (!running_[cpu_id]) dispatch_to_cpu(cpu_id);
  }
}

// === Main Loop ===
void Scheduler::tick_loop() {
  std::lock_guard<std::mutex> lock(schedulerMtx_);

  if (this->job_queue_.isEmpty() && this->ready_queue_.isEmpty()) {
    return;
  }

  // Middle-term scheduling: handle page faults, swapping
  // empty for now

  // Long-term scheduling: admit new jobs
  if (!this->job_queue_.isEmpty()) {
    Scheduler::long_term_admission();
  }

  // Short-term scheduling: dispatch to CPUs
  if (!this->ready_queue_.isEmpty()) {
    Scheduler::short_term_dispatch();
  }

  std::this_thread::sleep_for(std::chrono::milliseconds(cfg_.scheduler_tick_delay));
}


std::string Scheduler::snapshot() {
  return ""; 
}


uint32_t Scheduler::current_tick() const { return tick_.load(); }
