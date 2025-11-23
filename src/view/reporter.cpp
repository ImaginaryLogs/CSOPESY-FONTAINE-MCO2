#include "view/reporter.hpp"
#include "kernel/scheduler.hpp"
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <ctime>

Reporter::Reporter(Scheduler &sched) : sched_(sched) {}

static std::string now_string() {
  std::time_t t = std::time(nullptr);
  std::tm tm_buf{};
#ifdef _WIN32
  localtime_s(&tm_buf, &t);
#else
  localtime_r(&t, &tm_buf);
#endif
  char buf[64];
  std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm_buf);
  return std::string(buf);
}

// derive utilization by counting idle lines from the scheduler state snapshot
static void derive_utilization(Scheduler &sched, int &used, int &total) {
    auto u = sched.cpu_utilization();
    used = u.used;
    total = u.total;
}

std::string Reporter::build_report() {
  
  int used=0, total=0;
  derive_utilization(sched_, used, total);
  int available = std::max(0, total - used);
  int percent = (total>0) ? (used*100/total) : 0;

  std::ostringstream oss;
  std::cout << "HAHAHA1\n";
  oss << "CPU utilization: " << percent << "%\n"
      << "Cores used: " << used << "\n"
      << "Cores available: " << available << "\n\n"
      << sched_.snapshot() << "\n";
  return oss.str();
}

void Reporter::write_log(const std::string &path) {
  std::ofstream out(path, std::ios::app);
  if (!out) return;
  out << "===== Report at " << now_string() << " =====\n"
      << build_report()
      << "============================================\n\n";
}
