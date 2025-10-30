#pragma once
#include "config.hpp"
#include "cpu_worker.hpp"
#include "process_generator.hpp"
#include "reporter.hpp"
#include "scheduler.hpp"
#include "screen.hpp"

class CLI {
public:
  CLI();
  int run(); // main loop; returns exit code
private:
  void handle_command(const std::string &cmd);
  Config cfg_;
  bool initialized_{false};
  Scheduler *scheduler_{nullptr};
  std::vector<std::unique_ptr<CPUWorker>> cpu_workers_;
  ScreenManager screen_mgr_;
  ProcessGenerator *generator_{nullptr};
  Reporter *reporter_{nullptr};
};
