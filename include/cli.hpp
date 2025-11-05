#pragma once
#include "config.hpp"
#include "cpu_worker.hpp"
#include "process_generator.hpp"
#include "reporter.hpp"
#include "scheduler.hpp"
#include "screen.hpp"

#ifndef MY_HEADER_FILE_H
#define MY_HEADER_FILE_H
class CLI {
public:
  CLI();
  ~CLI();
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
  private:
  bool require_init() const;
  void initialize_system();
  void handle_screen_command(const std::vector<std::string>& args);
  void attach_process_screen(const std::string& name, const std::shared_ptr<Process>& proc);
};

#endif 