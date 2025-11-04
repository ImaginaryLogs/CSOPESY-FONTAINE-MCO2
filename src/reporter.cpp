#include "../include/reporter.hpp"
#include "../include/scheduler.hpp"
#include <fstream>
#include <iomanip>
#include <sstream>
#include <ctime>

Reporter::Reporter(Scheduler &sched) : sched_(sched) {}

// Return current local timestamp
static std::string now_string() {
  std::time_t t = std::time(nullptr);
  std::tm tm_buf{};
#ifdef _WIN32
  localtime_s(&tm_buf, &t);
#else
  localtime_r(&t, &tm_buf);
#endif
  std::ostringstream oss;
  oss << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S");
  return oss.str();
}

// Build report text of scheduler state
std::string Reporter::build_report() {
  std::ostringstream oss;
  oss << "CPU / Process Report\n"
      << "Timestamp: " << now_string() << "\n\n"
      << sched_.snapshot() << "\n";
  return oss.str();
}

// Append report to file with timestamp header
void Reporter::write_log(const std::string &path) {
  std::ofstream out(path, std::ios::app);
  if (!out) return;

  out << "===== Report at " << now_string() << " =====\n"
      << build_report()
      << "============================================\n\n";
}
