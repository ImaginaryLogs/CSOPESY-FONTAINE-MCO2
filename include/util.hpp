#pragma once
#include <cstdint>
#include <string>
#include <set>
#include <functional>
#include "config.hpp"
#include "process.hpp"

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

  protected:
    std::deque<T> q_; // dequeue for future implementation

  private:
    std::mutex messageMtx_;
    std::condition_variable messageCv_;
};



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
  inline bool operator>(const TimerEntry& other) const;
};

