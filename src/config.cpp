#include "../include/config.hpp"
#include <fstream>
#include <sstream>
#include <string>
#include <algorithm>
#include <iostream>

static std::string trim(std::string s) {
  auto notspace = [](int ch){ return !std::isspace(ch); };
  s.erase(s.begin(), std::find_if(s.begin(), s.end(), notspace));
  s.erase(std::find_if(s.rbegin(), s.rend(), notspace).base(), s.end());
  return s;
}

Config load_config(const std::string &path) {

  Config cfg{};
  cfg.scheduler_tick_delay = 0;
  std::ifstream in(path);

  if (!in) {
    // keep defaults if file missing
    return cfg;
  }

  std::string key, value;

  while (in >> key >> value)
  {
    key = trim(key), value = trim(value);

    if (key == "num-cpu")
      cfg.num_cpu = static_cast<uint32_t>(std::stoul(value));

    else if (key == "scheduler") {
      std::string v=value; std::transform(v.begin(), v.end(), v.begin(), ::tolower);
      if (v == "rr")        cfg.scheduler = SchedulingPolicy::RR;
      else if (v == "fcfs") cfg.scheduler = SchedulingPolicy::FCFS;
      else cfg.scheduler = SchedulingPolicy::FCFS;
    }

    else if (key == "quantum-cycles") cfg.quantum_cycles = static_cast<uint32_t>(std::stoul(value));
    else if (key == "batch-process-freq") cfg.batch_process_freq = static_cast<uint32_t>(std::stoul(value));
    else if (key == "min-ins") cfg.min_ins = static_cast<uint32_t>(std::stoul(value));
    else if (key == "max-ins") cfg.max_ins = static_cast<uint32_t>(std::stoul(value));
    else if (key == "delay-per-exec") cfg.delay_per_exec = static_cast<uint32_t>(std::stoul(value));
    else if (key == "snapshot-cooldown") cfg.snapshot_cooldown = static_cast<uint32_t>(std::stoul(value));
    else if (key == "scheduler-tick-delay") cfg.scheduler_tick_delay = static_cast<uint32_t>(std::stoul(value));
    else if (key == "max-generated-processes") cfg.max_generated_processes = static_cast<uint32_t>(std::stoul(value));
    else if (key == "save-snapshot-file-rate") cfg.save_snapshot_file_rate = static_cast<uint32_t>(std::stoul(value));
    else if (key == "remove-finished") cfg.remove_finished = static_cast<uint32_t>(std::stoul(value));
    else if (key == "remove-finished-capacity") cfg.remove_finished_capacity = static_cast<uint32_t>(std::stoul(value));

    // Memory Management
    else if (key == "max-overall-mem") cfg.max_overall_mem = static_cast<uint32_t>(std::stoul(value));
    else if (key == "mem-per-frame") cfg.mem_per_frame = static_cast<uint32_t>(std::stoul(value));
    else if (key == "min-mem-per-proc") cfg.min_mem_per_proc = static_cast<uint32_t>(std::stoul(value));
    else if (key == "max-mem-per-proc") cfg.max_mem_per_proc = static_cast<uint32_t>(std::stoul(value));
  }

  // Validation
  // Check if max-overall-mem and mem-per-frame are powers of 2
  auto is_power_of_2 = [](uint32_t n) { return n > 0 && (n & (n - 1)) == 0; };

  if (!is_power_of_2(cfg.max_overall_mem)) {
      std::cerr << "Warning: max-overall-mem (" << cfg.max_overall_mem << ") is not a power of 2. Adjusting to nearest power of 2 not implemented, please fix config.\n";
  }
  if (!is_power_of_2(cfg.mem_per_frame)) {
      std::cerr << "Warning: mem-per-frame (" << cfg.mem_per_frame << ") is not a power of 2.\n";
  }

  return cfg;
}
