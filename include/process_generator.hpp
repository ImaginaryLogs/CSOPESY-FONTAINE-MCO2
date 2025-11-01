#pragma once
#include "config.hpp"
#include "instruction.hpp"
#include "scheduler.hpp"
#include <atomic>
#include <thread>
#include <vector>

// May also be better as a Singleton as only one generator is needed
class ProcessGenerator {
public:
  ProcessGenerator(const Config &cfg, Scheduler &sched);
  void start();
  void stop();

  // Generate up to target_top_level top-level instructions while respecting
  // the configured max_unrolled_instructions budget. Returns the generated
  // instructions and writes the estimated unrolled size to estimated_size.
  std::vector<Instruction> generate_instructions(uint32_t target_top_level,
                                                 uint32_t &estimated_size);

private:
  void loop();
  Config cfg_;
  Scheduler &sched_;
  std::thread thread_;
  std::atomic<bool> running_{false};
  std::atomic<uint32_t> next_id_{1};
};
