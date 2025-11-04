#include "../include/util.hpp"
#include "../include/process.hpp"

#include <cstdint>
#include <string>
#include <set>
#include <map>
#include <functional>
#include <memory>
#include <sstream>

template class Channel<std::string>;
template class Channel<std::shared_ptr<Process>>;

template<typename T>
void Channel<T>::send(const T& message) {
  {
    std::lock_guard<std::mutex> lock(messageMtx_);
    this->q_.emplace_back(message);
  }
  messageCv_.notify_one();
}

template<typename T>
T Channel<T>::receive() {
  std::unique_lock<std::mutex> lock(messageMtx_);
  messageCv_.wait(lock, [this]{ return !q_.empty(); });
  T message = q_.front();
  q_.pop_front();
  return message;
}

template<typename T>
bool Channel<T>::isEmpty() {
  std::lock_guard<std::mutex> lock(messageMtx_);
  return q_.empty();
}

template<typename T>
void Channel<T>::empty(){
  std::lock_guard<std::mutex> lock(messageMtx_);
  q_.empty();
  return;
}


template<>
inline std::string Channel<std::string>::snapshot(){
  std::lock_guard<std::mutex> lock(messageMtx_);
  std::ostringstream oss;
  for (const auto &msg : this->q_){
    oss << msg;
  }
  return oss.str();
}

template<>
inline std::string Channel<std::shared_ptr<Process>>::snapshot() {
    std::lock_guard<std::mutex> lock(messageMtx_);
    std::ostringstream oss;
    oss << "Channel Snapshot: " << q_.size() << " messages\n";
    for (const auto &msg : q_) {
        if (msg) {
            oss << "  ID: " << msg->id()
                << ", PRIO: " << msg->priority
                << ", PC: " << msg->pc
                << ", STATE: " << msg->get_state_string()
                << ", NAME: " << msg->name()
                << "\n";
        } else {
            oss << "nullptr\n";
        }
    }
    return oss.str();
}

bool TimerEntry::operator>(const TimerEntry& other) const {
    return this->wake_tick > other.wake_tick;
}

void FinishedMap::insert(ProcessPtr p, uint32_t finished_tick){
  std::scoped_lock<std::mutex> lock(this->mutex_);
  std::string name = p->name();

  if (finished_by_name_.count(name)){
    rename_process(p);
  } else {
    duplicate_count_[name] = 0;
  }

  finished_by_name_[name] = p;
  finished_by_time_.emplace(finished_tick, p);
};

ProcessPtr FinishedMap::get_by_name(const std::string& name) {
    std::scoped_lock<std::mutex> lock(this->mutex_);
    auto it = finished_by_name_.find(name);
    return (it != finished_by_name_.end()) ? it->second : nullptr;
}

// Check if process name exists
bool FinishedMap::contains(const std::string& name) {
    std::scoped_lock<std::mutex> lock(mutex_);
    return finished_by_name_.find(name) != finished_by_name_.end();
}

// Return ordered list of finished processes (most recent first)
std::vector<std::pair<uint32_t, ProcessPtr>> FinishedMap::ordered() {
    std::scoped_lock<std::mutex> lock(mutex_);
    std::vector<std::pair<uint32_t, ProcessPtr>> result;
    result.reserve(finished_by_time_.size());

    for (auto& [tick, weak] : finished_by_time_) {
        if (auto proc = weak.lock())
            result.emplace_back(tick, proc);
    }
    return result;
}

// Clear all finished processes
void FinishedMap::clear() {
    std::scoped_lock<std::mutex> lock(mutex_);
    finished_by_name_.clear();
    finished_by_time_.clear();
    duplicate_count_.clear();
}

// Count
size_t FinishedMap::size() {
    std::scoped_lock<std::mutex> lock(mutex_);
    return finished_by_name_.size();
}


std::string FinishedMap::snapshot() {
  auto ordered_list = ordered();
  std::ostringstream oss;
  for (const auto& pair : ordered_list)
      oss << pair.second->name()
                << " (finished at tick " << pair.first << ") "
                << pair.second->get_executed_instructions() << " / "
                << pair.second->get_total_instructions() << "\n";
  return oss.str();
}