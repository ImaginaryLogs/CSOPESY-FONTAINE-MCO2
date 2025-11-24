#pragma once
#include <memory>
#include <thread>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <string>
#include <cstdint>


// Forward declaration to break circular dependency
class Scheduler;

class CPUWorker {
public:
  CPUWorker(uint32_t id, Scheduler &sched);
  void start();
  void stop();
  void join();

private:
  void loop();
  uint32_t id_;                       // CPU ID
  Scheduler &sched_;                  // Assigned Scheduler
  std::thread thread_;                // Assigned Thread
  std::atomic<bool> running_{false};  // Is CPU Worker Running?
};
