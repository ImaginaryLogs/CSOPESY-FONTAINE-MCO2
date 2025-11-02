#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include "../include/util.hpp"

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
  q_.pop();
  return message;
}

template<typename T>
bool Channel<T>::isEmpty() {
  std::lock_guard<std::mutex> lock(messageMtx_);
  return q_.empty();
}
