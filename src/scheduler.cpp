#include "../include/scheduler.hpp"

Scheduler::Scheduler(const Config &cfg)
    : cfg_(cfg), running_processes_(cfg.num_cpu),
      busy_ticks_per_cpu_(cfg.num_cpu) {}

Scheduler::~Scheduler() {
  if (running_.load()) {
    stop();
  }
}

void Scheduler::start() {
  if (running_.load())
    return;

    this->cfg_ = initial_cfg;
  this->sched_running_ = false;
  this->tick_ = 0;

  InitializeQueues();
  InitializeVectors();

  this->busy_ticks_per_cpu_ = std::vector<uint64_t>();
  this->cpu_quantum_remaining_ = std::unordered_map<uint32_t, uint32_t>();

  for (uint32_t i = 0; i < cfg_.num_cpu; i++) {
    busy_ticks_per_cpu_.push_back(0);
    cpu_quantum_remaining_[i] = 0;
  }
  
  running_.store(true);
  sched_thread_ = std::thread(&Scheduler::tick_loop, this);
}

void Scheduler::stop() {
  running_.store(false);
  if (sched_thread_.joinable()) {
    sched_thread_.join();
  }
}

void Scheduler::submit_process(std::shared_ptr<Process> p) {
  std::lock_guard<std::mutex> lock(mtx_);
  ready_queue_.push_back(p);
  cv_.notify_one();
}

std::string Scheduler::snapshot() {
  return ""; // TODO: Implement
}

std::shared_ptr<Process> Scheduler::dispatch_to_cpu(uint32_t cpu_id) {
  return nullptr; // TODO: Implement
}

void Scheduler::release_cpu(uint32_t cpu_id, std::shared_ptr<Process> p,
                            bool finished, bool yielded) {
  // TODO: Implement
}

uint32_t Scheduler::current_tick() const { return tick_.load(); }

void Scheduler::tick_loop() {
  // TODO: Implement
}
