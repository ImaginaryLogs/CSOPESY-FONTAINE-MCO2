#pragma once
#include <cstdint>
#include <string>


enum Scheduling_Algorithm {
  RR,
  FCFS
};

struct Config {
  uint32_t num_cpu = 4;
  Scheduling_Algorithm scheduler = RR; // "rr" or "fcfs"
  uint32_t quantum_cycles = 5;
  uint32_t batch_process_freq = 1;
  uint32_t min_ins = 1000;
  uint32_t max_ins = 2000;
  uint32_t delay_per_exec = 0;
  // Maximum total instructions after FOR unrolling (0 = no limit)
  uint32_t max_unrolled_instructions = 10000;
};

Config load_config(const std::string &path);
