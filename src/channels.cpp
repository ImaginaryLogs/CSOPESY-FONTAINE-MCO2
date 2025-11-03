#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <sstream>
#include "../include/util.hpp"


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
  for (const auto &msg : q_){
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
