#include "../include/config.hpp"
#include <fstream>
#include <sstream>
#include <string>
#include <algorithm>

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
  }
  return cfg;
}
