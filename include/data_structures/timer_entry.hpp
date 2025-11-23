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
#include <cassert>
#include <queue>
#include "processes/process.hpp"

// An Entry of a Process for the Timer to check
struct TimerEntry {
  std::shared_ptr<Process> process;
  uint64_t wake_tick;
  
  inline bool operator>(const TimerEntry& other) const {
      return wake_tick > other.wake_tick;
  }

  inline bool operator<(const TimerEntry& other) const {
      return wake_tick < other.wake_tick;
  }
};

struct TimerEntryCompare {
    bool operator()(const TimerEntry& a, const TimerEntry& b) const noexcept {
        return a.wake_tick > b.wake_tick; // min-heap: smallest wake_tick first
    }
};


class TimerEntrySleepQueue {
  public:
    std::vector<TimerEntry> get_sleep_queue_snapshot() const;
    std::string print_sleep_queue() const;
    void send(std::shared_ptr<Process> p, uint64_t wake_tick) ;
    TimerEntry receive();
    bool isEmpty() const;
    TimerEntry top() const;
  private:
    std::priority_queue<TimerEntry> sleep_queue_;
    mutable std::mutex sleep_queue_mtx_;
};

