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
#include "process.hpp"
#include "util.hpp"
#include <cassert>
#include <queue>

template<typename T>
class BufferedChannel {
  public:
    void send(const T& message);
    
    T receive();

    bool isEmpty();
    std::string snapshot();
    void setCapacity();
    void setOverwrite();

  protected:
    std::deque<T> q_; // dequeue for future implementation

  private:
    std::mutex messageMtx_;
    std::condition_variable messageCv_;
    bool has_overwrite_;
    size_t cap;
};

