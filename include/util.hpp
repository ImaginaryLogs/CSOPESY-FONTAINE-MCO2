#pragma once
#include <cstdint>
#include <string>
#include <set>
#include "config.hpp"
#include "process.hpp"

uint16_t clamp_uint16(int64_t v);
std::string now_iso();

// Thread Safe Queues (or channels in go-lang) are a VERY common data structure in schedulers.
// They often require synchronization for thread safety.
// Might as well implement a thread-safe queue here.

template<typename T>
class Channel {
  public:
    void send(const T& message);
    
    T receive();

    bool isEmpty();
    
  protected:
    std::deque<std::shared_ptr<T>> q_; // dequeue for future implementation

  private:
    std::mutex messageMtx_;
    std::condition_variable messageCv_;
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

  protected:
    std::multiset<std::shared_ptr<Process>, ProcessComparer> victimQ_;
  
  private:
    SchedulingPolicy policy_;
    ProcessComparer comparator_;
    std::mutex messageMtx_;
    std::condition_variable messageCv_;
};