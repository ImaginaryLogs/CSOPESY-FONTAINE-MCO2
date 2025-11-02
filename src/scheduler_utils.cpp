#include "../include/scheduler.hpp"
#include "../include/process.hpp"
#include <sstream>

// This files just contains Scheduler's Utility functions


void Scheduler::initialize_vectors() {
  this->running_ = std::vector<std::shared_ptr<Process>>(cfg_.num_cpu, nullptr);
  this->finished_ = std::vector<std::shared_ptr<Process>>(cfg_.num_cpu, nullptr);
  this->busy_ticks_per_cpu_ = std::vector<uint64_t>();
  this->cpu_quantum_remaining_ = std::vector<uint32_t>();
}

// === SHORT TERM SCHEDULER ALGORITHM IMPLEMENTATION ===
bool ProcessComparer::operator()(const std::shared_ptr<Process>& a, const std::shared_ptr<Process>& b) const {
  // Default comparison (by process ID)
  return a->id() < b->id();
}

struct FCFSAlgo : ProcessComparer {
  bool operator()(const std::shared_ptr<Process>& a, const std::shared_ptr<Process>& b) const {
    return a->last_active_tick == b->last_active_tick      // are both same arrival time?
            ? a->id() < b->id()                             // yes, tie-breaker by ID  
            : a->last_active_tick < b->last_active_tick;    // no, earlier arrival first;
  }
};

struct PriorityAlgo : ProcessComparer {
  bool operator()(const std::shared_ptr<Process>& a, const std::shared_ptr<Process>& b) const {
    return a->priority == b->priority // Are both same priority?
      ? a->id() < b->id()             // yes, tie-breaker by ID
      : a->priority > b->priority;    // no, Higher priority value means higher priority
  }
};

struct RRAlgo : ProcessComparer {
  bool operator()(const std::shared_ptr<Process>& a, const std::shared_ptr<Process>& b) const {
    return a->last_active_tick == b->last_active_tick       // are both same arrival time?
            ? a->id() < b->id()                             // yes, tie-breaker by ID  
            : a->last_active_tick < b->last_active_tick;    // no, earlier arrival first;
  }
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
      comparator_ = RRAlgo();
      break;
    case FCFS:
      comparator_ = FCFSAlgo();
      break;
    case PRIORITY:
      comparator_ = PriorityAlgo();
    default:
      comparator_ = ProcessComparer(); // Default comparer
      break;  
  }
  std::multiset<std::shared_ptr<Process>, ProcessComparer> newQueue(comparator_);
  newQueue.insert(victimQ_.begin(), victimQ_.end());
  victimQ_.swap(newQueue);
}


DynamicVictimChannel::DynamicVictimChannel(SchedulingPolicy algo)
    : policy_(algo) {
  DynamicVictimChannel::reformatQueue();
}

std::string DynamicVictimChannel::snapshot() {
  std::lock_guard<std::mutex> lock(messageMtx_);
  std::stringstream ss;

  ss << "DVC Snapshot: " << victimQ_.size() << " processes\n";
  for (const auto &proc : victimQ_) {
      ss << "PID=" << proc->id() << ", Name=" << proc->name() << ", " << "\n";
  }
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
