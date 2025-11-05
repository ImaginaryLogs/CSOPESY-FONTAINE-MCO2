#include "../include/finished_map.hpp"
#include "../include/process.hpp"
#include <sstream>
#include <iostream>
#include <iomanip>
#include <ctime>

void FinishedMap::insert(ProcessPtr p, uint32_t finished_tick){
  std::scoped_lock<std::mutex> lock(this->mutex_);
  std::string name = p->name();

  if (finished_by_name_.count(name)) {
      rename_process(p);
  } else {
      duplicate_count_[name] = 0;
  }

  time_t now = std::time(nullptr);

  finished_by_name_[name] = p;
  finished_by_tick_.emplace(finished_tick, p);
  finished_by_time_.emplace(now, p);
};

ProcessPtr FinishedMap::get_by_name(const std::string& name) {
    std::scoped_lock<std::mutex> lock(this->mutex_);
    auto it = finished_by_name_.find(name);
    return (it != finished_by_name_.end()) ? it->second : nullptr;
}

// Check if process name exists
bool FinishedMap::contains(const std::string& name) {
    std::scoped_lock<std::mutex> lock(mutex_);
    return finished_by_name_.find(name) != finished_by_name_.end();
}

// Return ordered list of finished processes (most recent first)
std::vector<std::tuple<uint32_t, ProcessPtr, time_t>> FinishedMap::ordered() {
  std::scoped_lock<std::mutex> lock(mutex_);
  std::vector<std::tuple<uint32_t, ProcessPtr, time_t>> result;
  result.reserve(finished_by_tick_.size());

  // Match each process' tick with its finish time
  for (auto& [tick, weak] : finished_by_tick_) {
      if (auto proc = weak.lock()) {
          // Look up its recorded time (if found)
          time_t t = 0;
          for (auto it = finished_by_time_.begin(); it != finished_by_time_.end(); ++it) {
              if (auto p2 = it->second.lock()) {
                  if (p2 == proc) {
                      t = it->first;
                      break;
                  }
              }
          }
          result.emplace_back(tick, proc, t);
      }
  }
  return result;
}

// Clear all finished processes
void FinishedMap::clear() {
    std::scoped_lock<std::mutex> lock(mutex_);
    finished_by_name_.clear();
    finished_by_tick_.clear();
    duplicate_count_.clear();
}

// Count
size_t FinishedMap::size() {
    std::scoped_lock<std::mutex> lock(mutex_);
    return finished_by_name_.size();
}


std::string FinishedMap::snapshot() {
  auto ordered_list = ordered();
  std::ostringstream oss;
  uint16_t counter = 0;

  oss << "Name\tFinished Time\tTick\tProgress\t#\n";
  oss << "------------------------------------------------------\n";

  for (const auto& [tick, proc, finish_time] : ordered_list) {
      std::tm tm_buf{};
      localtime_r(&finish_time, &tm_buf);

      oss << proc->name() << "\t"
          << std::put_time(&tm_buf, "%d-%m-%Y %H:%M:%S") << "\t"
          << "(TICK " << tick << ")\t"
          << proc->get_executed_instructions() << " / "
          << proc->get_total_instructions() << "\t"
          << counter++ << "\n";
  }

  return oss.str();
}

// Helper: rename process if duplicate
std::string FinishedMap::rename_process(std::shared_ptr<Process> process) {
    std::string name = process->name();

    if (finished_by_name_.count(name)) {
        uint32_t dupIndex = ++duplicate_count_[name];
        std::ostringstream oss;
        oss << name << "_(" << dupIndex << ")";
        name = oss.str();
        process->set_name(name);
    } else {
        duplicate_count_[name] = 0;
    }

    return name;
}