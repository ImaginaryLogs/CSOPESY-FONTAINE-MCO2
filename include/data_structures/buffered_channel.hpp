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
#include "config.hpp"
#include "util.hpp"
#include <cassert>
#include <queue>

template<typename T>
class BufferedChannel {
  public:
    BufferedChannel(size_t capacity = 1, bool overwrite = false)
        : cap_(capacity), has_overwrite_(overwrite) {};
    void send(const T& message);
    T receive() ;
    bool isEmpty();
    std::string snapshot();
    void setCapacity(size_t newCap);
    void setOverwrite(bool v);
    size_t size();
  private:
    std::deque<T> q_;
    size_t cap_;
    bool has_overwrite_;

    std::mutex messageMtx_;
    std::condition_variable messageCvEmpty_; // waiting for messages
    std::condition_variable messageCvFull_;  // waiting for space
};


template<typename T> 
size_t BufferedChannel<T>::size(){
  return this->q_.size();
}


template<typename T> 
void BufferedChannel<T>::send(const T& message) {
  std::unique_lock<std::mutex> lock(messageMtx_);

  if (!has_overwrite_) {
      // Wait if full
      messageCvFull_.wait(lock, [&]() {
          return q_.size() < cap_;
      });
  } else {
      // Overwrite mode: if full, erase oldest
      if (q_.size() >= cap_) {
          q_.pop_front();
      }
  }

  // Add new message
  q_.push_back(message);

  // Notify receiver
  messageCvEmpty_.notify_one();
}

template<typename T> 
T BufferedChannel<T>::receive() {
  std::unique_lock<std::mutex> lock(messageMtx_);

  // Wait until non-empty
  messageCvEmpty_.wait(lock, [&]() {
      return !q_.empty();
  });

  T value = q_.front();
  q_.pop_front();

  // Notify senders waiting for space
  messageCvFull_.notify_one();

  return value;
}

template<typename T> 
bool BufferedChannel<T>::isEmpty() {
  std::lock_guard<std::mutex> lock(messageMtx_);
  return q_.empty();
}

template<typename T> 
std::string BufferedChannel<T>::snapshot() {
  std::lock_guard<std::mutex> lock(messageMtx_);

  std::ostringstream oss;
  oss << "[";

  bool first = true;
  for (const auto& item : q_) {
      if (!first) oss << ", ";
      first = false;
      oss << item;
  }

  oss << "]";
  return oss.str();
}

    // -------------------------------------------------------------
    // Set a new capacity. Safely shrinks queue if needed.
    // -------------------------------------------------------------
template<typename T> 
void BufferedChannel<T>::setCapacity(size_t newCap) {
  std::lock_guard<std::mutex> lock(messageMtx_);
  cap_ = newCap;

  // If new capacity is smaller, drop oldest items
  while (q_.size() > cap_) {
      q_.pop_front();
  }

  messageCvFull_.notify_all();
}

// -------------------------------------------------------------
template<typename T> 
void BufferedChannel<T>::setOverwrite(bool v) {
    std::lock_guard<std::mutex> lock(messageMtx_);
    has_overwrite_ = v;

    if (has_overwrite_) {
        // Wake senders so they don't block anymore
        messageCvFull_.notify_all();
    }
}
