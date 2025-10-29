#pragma once
#include "scheduler.hpp"
#include <string>

class Reporter {
public:
  Reporter(Scheduler &sched);
  std::string build_report(); // same content as screen-ls
  void write_log(const std::string &path);

private:
  Scheduler &sched_;
};
