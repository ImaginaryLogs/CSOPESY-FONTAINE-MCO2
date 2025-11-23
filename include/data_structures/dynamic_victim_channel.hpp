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
#include "processes/process.hpp"
#include "util.hpp"
#include <cassert>
#include <queue>

using ProcessPtr = std::shared_ptr<Process>;
using ProcessCmpFn = std::function<bool(const ProcessPtr&, const ProcessPtr&)>;



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
