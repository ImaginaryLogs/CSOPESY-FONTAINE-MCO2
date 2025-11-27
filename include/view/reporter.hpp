#pragma once
#include "kernel/scheduler.hpp"
#include <string>

class Reporter {
public:
  Reporter(Scheduler &sched);
  std::string build_report(); // same content as screen-ls
  std::string get_process_smi();
  std::string get_vmstat();
  void write_log(const std::string &path);

private:
  Scheduler &sched_;
};
