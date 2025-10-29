#pragma once
#include "config.hpp"
#include "scheduler.hpp"
#include <atomic>
#include <thread>

// May also be better as a Singleton as only one generator is needed
class ProcessGenerator {
public:
  ProcessGenerator(const Config &cfg, Scheduler &sched);
  void start();
  void stop();

private:
  void loop();
  Config cfg_;
  Scheduler &sched_;
  std::thread thread_;
  std::atomic<bool> running_{false};
  std::atomic<uint32_t> next_id_{1};
};
