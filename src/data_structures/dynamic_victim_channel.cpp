#include "data_structures/dynamic_victim_channel.hpp"
#include <iostream>

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

    uint16_t ui_showcount = 10;
    uint16_t count = 0;
    uint16_t top = victimQ_.size();
    if (!victimQ_.empty())
      ss << "Name\tPID\tLA\t#\n"
         << "----------------------------------------\n";
    for (const auto &proc : victimQ_) {
        if (count++ >= ui_showcount) break; // limit
        ss << proc->name() << "\t"
           << proc->id()  << "\t" 
           << proc->last_active_tick << "\t"
           << top << "\n";
        --top;
    }

    if (victimQ_.size() > ui_showcount)
        ss << "... (" << victimQ_.size() - ui_showcount << " more)\n";

    return ss.str();
}


std::string DynamicVictimChannel::print() {
    std::lock_guard<std::mutex> lock(messageMtx_);
    std::stringstream ss;

    uint16_t ui_showcount = 10;
    uint16_t top = victimQ_.size();
    if (!victimQ_.empty())
      ss << "Name\tPID\tLA\t#\n"
         << "----------------------------------------\n";
    for (const auto &proc : victimQ_) {
        ss << proc->name() << "\t"
           << proc->id()  << "\t" 
           << proc->last_active_tick << "\t"
           << top << "\n";
        --top;
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

// 
size_t DynamicVictimChannel::size(){
  std::lock_guard<std::mutex> lock(messageMtx_);
  return victimQ_.size();
}
