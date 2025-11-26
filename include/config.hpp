#pragma once
#include <cstdint>
#include <string>

enum SchedulingPolicy {
  RR,
  FCFS,
  PRIORITY
};

struct Config {
  uint32_t num_cpu = 4;
  SchedulingPolicy scheduler = FCFS; // "rr" or "fcfs"
  uint32_t quantum_cycles = 5;
  uint32_t batch_process_freq = 1;
  uint32_t min_ins = 1000;
  uint32_t max_ins = 2000;
  uint32_t delay_per_exec = 0;
  uint32_t scheduler_tick_delay = 100;
  // Maximum total instructions after FOR unrolling (0 = no limit)
  uint32_t max_unrolled_instructions = 10000;
  uint32_t snapshot_cooldown = 20;
  uint32_t max_generated_processes = 20;
  uint32_t save_snapshot_file_rate = 50;
  uint32_t remove_finished = 1;
  uint32_t remove_finished_capacity = 5000;
};

Config load_config(const std::string &path);
