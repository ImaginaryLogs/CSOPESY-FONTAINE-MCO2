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

uint16_t clamp_uint16(int64_t v);
std::string now_iso();

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


struct CpuUtilization {
    unsigned used;
    unsigned total;
    double percent;
    std::string to_string() const {
        std::ostringstream oss;
        oss << static_cast<int>(percent) << "%";
        return oss.str();
    }
};