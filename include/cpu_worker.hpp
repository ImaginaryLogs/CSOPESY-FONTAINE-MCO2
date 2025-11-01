#pragma once
#include "scheduler.hpp"
#include <thread>


class CPUWorker {
public:
  CPUWorker(uint32_t id, Scheduler &sched);
  void start();
  void stop();
  void join();

private:
  void loop();
  uint32_t id_;
  Scheduler &sched_;
  std::thread thread_;
  std::atomic<bool> running_{false};

};
