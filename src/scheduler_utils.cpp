#include "../include/scheduler.hpp"
#include "../include/process.hpp"
#include <sstream>
#include <iostream>

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

void Scheduler::tick_barrier_sync()
{
  this->tick_sync_barrier_->arrive_and_wait();
}

uint32_t Scheduler::get_cpu_count() const { return cfg_.num_cpu; };

// uint32_t Scheduler::count_running_cores() const {
//   std::lock_guard<std::mutex> lock(short_term_mtx_);
//   uint32_t used = 0;
//   for (const auto &p : running_) if (p) ++used;
//   return used;
// }


uint32_t Scheduler::get_scheduler_tick_delay() const { return cfg_.scheduler_tick_delay; }


// === SHORT TERM SCHEDULER ALGORITHM IMPLEMENTATION ===
bool ProcessComparer::operator()(const std::shared_ptr<Process>& a, const std::shared_ptr<Process>& b) const {
  // Default comparison (by process ID)
  std::cout << "Default Algo";
  return a->id() > b->id();
}

ProcessCmpFn fcfs_cmp = [](const ProcessPtr &a, const ProcessPtr &b) {
  if (a->last_active_tick == b->last_active_tick) return a->id() < b->id();
  return a->last_active_tick < b->last_active_tick;
};

ProcessCmpFn rr_cmp = [](const ProcessPtr &a, const ProcessPtr &b) {
  if (a->last_active_tick == b->last_active_tick) return a->id() < b->id();
  return a->last_active_tick < b->last_active_tick;
};

ProcessCmpFn prio_cmp = [](const ProcessPtr &a, const ProcessPtr &b) {

  if (a->priority == b->priority) return a->id() < b->id();
  return a->priority > b->priority;
};

// === DynamicVictimChannel Implementation ===

void DynamicVictimChannel::setPolicy(SchedulingPolicy algo) {
  policy_ = algo;
  reformatQueue();
}

// Rebuild the victimQ_ based on current comparator_
void DynamicVictimChannel::reformatQueue() {
  std::lock_guard<std::mutex> lock(messageMtx_);
  switch (policy_) {
    case RR:
      comparator_ = rr_cmp;
      break;
    case FCFS:
      comparator_ = fcfs_cmp;
      break;
    case PRIORITY:
      comparator_ = prio_cmp;
      break;
    default:
      comparator_ = fcfs_cmp; // Default comparer
      break;  
  }
  std::multiset<std::shared_ptr<Process>, ProcessCmpFn> newQueue(comparator_);
  newQueue.insert(victimQ_.begin(), victimQ_.end());
  victimQ_.swap(newQueue);
  this->comparator_ = comparator_;
}

DynamicVictimChannel::DynamicVictimChannel(SchedulingPolicy algo)
    : policy_(algo) {
  DynamicVictimChannel::reformatQueue();
}

std::string DynamicVictimChannel::snapshot() {
    std::lock_guard<std::mutex> lock(messageMtx_);
    std::stringstream ss;

  
    uint16_t count = 0;
    for (const auto &proc : victimQ_) {
        if (count++ >= 10) break; // limit
        ss << proc->name() << "\t"
           << "PID=" << proc->id()  << "\t" 
           << "LA=" << proc->last_active_tick << "\n";
    }

    if (victimQ_.size() > 10)
        ss << "... (" << victimQ_.size() - 10 << " more)\n";

    return ss.str();
}


void DynamicVictimChannel::send(const std::shared_ptr<Process> &msg) {
  {
    std::lock_guard<std::mutex> lock(messageMtx_);
    this->victimQ_.insert(msg);
  }
  messageCv_.notify_one();
}

std::shared_ptr<Process> DynamicVictimChannel::receiveNext() {
  std::unique_lock<std::mutex> lock(messageMtx_);
  messageCv_.wait(lock, [this]{ return !victimQ_.empty(); });
  auto it = victimQ_.begin();
  std::shared_ptr<Process> msg = *it;
  victimQ_.erase(it);
  return msg;
}

std::shared_ptr<Process> DynamicVictimChannel::receiveVictim() {
  std::unique_lock<std::mutex> lock(messageMtx_);
  messageCv_.wait(lock, [this]{ return !victimQ_.empty(); });
  auto it = std::prev(victimQ_.end()); // 
  std::shared_ptr<Process> msg = *it;
  victimQ_.erase(it);
  return msg;
}

// Accessor
bool DynamicVictimChannel::isEmpty() {
  std::lock_guard<std::mutex> lock(messageMtx_);
  return victimQ_.empty();
}
