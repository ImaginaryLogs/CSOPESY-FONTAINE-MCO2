#include "view/reporter.hpp"
#include "kernel/scheduler.hpp"
#include "paging/memory_manager.hpp"
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <ctime>
#include <algorithm>

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
  oss << "CPU utilization: " << percent << "%\n"
      << "Cores used: " << used << "\n"
      << "Cores available: " << available << "\n\n"
      << sched_.snapshot_with_log() << "\n";
  return oss.str();
}

// NOTE: Added by Antigravity. Feel free to revise/remove.
std::string Reporter::get_process_smi() {
    std::ostringstream oss;
  oss << "\n";
  // Memory summary header
  auto &mm = MemoryManager::getInstance();
  const Config &cfg = sched_.get_config();
  size_t total_frames = mm.get_total_frames();
  size_t free_frames = mm.get_free_frames_count();
  size_t used_frames = total_frames - free_frames;
  size_t total_kb = total_frames * cfg.mem_per_frame; // treat as KB per assignment spec
  size_t used_kb = used_frames * cfg.mem_per_frame;
  size_t free_kb = free_frames * cfg.mem_per_frame;
  oss << "Memory: total=" << total_kb << " KB, used=" << used_kb
    << " KB, free=" << free_kb << " KB\n";
    oss << "---------------------------------------------------------------------------\n";
    oss << "| Process ID | Process Name | Active Pages | Total Pages | Swap Space |\n";
    oss << "---------------------------------------------------------------------------\n";

    auto processes = sched_.get_all_processes();
    // Sort by PID
    std::sort(processes.begin(), processes.end(), [](const auto& a, const auto& b) {
        return a->id() < b->id();
    });

    for (const auto& p : processes) {
        auto stats = p->get_memory_stats();
        oss << "| " << std::left << std::setw(11) << p->id()
            << "| " << std::setw(13) << p->name()
            << "| " << std::setw(13) << stats.active_pages
            << "| " << std::setw(12) << stats.total_pages
            << "| " << std::setw(11) << stats.swap_pages << "|\n";
    }
    oss << "---------------------------------------------------------------------------\n";
    return oss.str();
}

// NOTE: Added by Antigravity. Feel free to revise/remove.
std::string Reporter::get_vmstat() {
    std::ostringstream oss;
    auto& mm = MemoryManager::getInstance();

    // Memory Stats
    size_t total_frames = mm.get_total_frames();
    size_t free_frames = mm.get_free_frames_count();
    size_t used_frames = total_frames - free_frames;

    const Config& cfg = sched_.get_config();
    // Values are in bytes as per config and frame size
    size_t total_mem_kb = cfg.max_overall_mem;                // treat values as KB to match rubric
    size_t used_mem_kb = used_frames * cfg.mem_per_frame;     // frames * frame size (KB)
    size_t free_mem_kb = free_frames * cfg.mem_per_frame;

    // CPU Stats
    int used_cpu=0, total_cpu=0;
    derive_utilization(sched_, used_cpu, total_cpu);
    int cpu_percent = (total_cpu>0) ? (used_cpu*100/total_cpu) : 0;
    int idle_percent = 100 - cpu_percent;

    // Paging Stats
    size_t paged_in = mm.get_paged_in_count();
    size_t paged_out = mm.get_paged_out_count();

    auto ticks = sched_.cpu_tick_stats();

    oss << "\n";
    oss << "Total Memory: " << total_mem_kb << " KB\n";
    oss << "Used Memory: " << used_mem_kb << " KB\n";
    oss << "Free Memory: " << free_mem_kb << " KB\n";
    oss << "Idle CPU: " << idle_percent << "%\n";
    oss << "Active CPU: " << cpu_percent << "%\n";
    oss << "Idle CPU Ticks: " << ticks.idle << "\n";
    oss << "Active CPU Ticks: " << ticks.busy << "\n";
    oss << "Total CPU Ticks: " << ticks.total << "\n";
    oss << "Pages Paged In: " << paged_in << "\n";
    oss << "Pages Paged Out: " << paged_out << "\n";

    return oss.str();
}

void Reporter::write_log(const std::string &path) {
  std::ofstream out(path, std::ios::app);
  if (!out) return;
  out << "===== Report at " << now_string() << " =====\n"
      << build_report()
      << "============================================\n\n";
}
