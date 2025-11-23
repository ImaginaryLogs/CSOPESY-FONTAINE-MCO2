#pragma once
#include <cstdint>
#include <string>
#include <set>
#include <map>
#include <deque> 
#include <sstream>
#include <memory>
#include <functional>
#include <condition_variable>
#include "processes/process.hpp"
#include <queue>

// Thread Safe Queues (or channels in go-lang) are a VERY common data structure in schedulers.
// They often require synchronization for thread safety.
// Might as well implement a thread-safe queue here.

template<typename T>
class Channel {
  public:
    void send(const T& message);
    
    T receive();

    bool isEmpty();
    std::string snapshot();
    void empty();

  private:
    std::deque<T> q_;
    std::mutex messageMtx_;
    std::condition_variable messageCv_;
};


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
  messageCv_.wait(lock, [this]() { return !q_.empty(); });
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

    uint16_t count = 0;
    for (const auto &msg : q_) {
        if (count++ >= 10) break; // limit

        if (msg) {
            oss << msg->name() << "\t"
                << "ID: " << msg->id() << "\t"
                << "PR: " << msg->priority << "\t"
                << "PC: " << msg->pc << "\t"
                << "(" << msg->get_state_string() << ")\t"
                << "\n";
        } else {
            oss << " None\n";
        }
    }

    if (q_.size() > 10)
        oss << "... (" << q_.size() - 10 << " more)\n";

    return oss.str();
}
