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

uint16_t clamp_uint16(int64_t v);
std::string now_iso();

using ProcessPtr = std::shared_ptr<Process>;
using ProcessCmpFn = std::function<bool(const ProcessPtr&, const ProcessPtr&)>;

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
  for (const auto &msg : q_) {
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
  return oss.str();
}

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




struct ProcessComparer {
  bool operator()(const std::shared_ptr<Process>& a, const std::shared_ptr<Process>& b) const;
};

// Ready Queue Implementation.
// A Channel that can also select a "victim" based on current scheduling policy.
// Check out `docs/scheduler.md` for design notes.

class DynamicVictimChannel {
  public:
    DynamicVictimChannel(SchedulingPolicy algo);

    // Algorithm Setting Methods
    void setPolicy(SchedulingPolicy algo);
    void reformatQueue();

    // Message Passing Methods
    void send(const std::shared_ptr<Process>& msg);
    std::shared_ptr<Process> receiveNext();
    std::shared_ptr<Process> receiveVictim();

    // Accessor
    bool isEmpty();
    std::string snapshot();

  protected:
    std::multiset<std::shared_ptr<Process>, ProcessCmpFn> victimQ_;
  
  private:
    SchedulingPolicy policy_;
    ProcessCmpFn comparator_;
    std::mutex messageMtx_;
    std::condition_variable messageCv_;
};

// An Entry of a Process for the Timer to check
struct TimerEntry {
    std::shared_ptr<Process> process;
    uint64_t wake_tick;

    TimerEntry(std::shared_ptr<Process> p, uint64_t tick)
        : process(std::move(p)), wake_tick(tick) {}

    TimerEntry() : process(nullptr), wake_tick(0) {}
};

struct TimerEntryCompare {
    bool operator()(const TimerEntry& a, const TimerEntry& b) const noexcept {
        return a.wake_tick > b.wake_tick; // min-heap: smallest wake_tick first
    }
};
