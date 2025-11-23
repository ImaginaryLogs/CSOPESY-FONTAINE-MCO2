#include "../include/finished_map.hpp"
#include <sstream>
#include <iomanip>
#include <ctime>
#include <bits/stdc++.h>

void FinishedMap::insert(ProcessPtr p, uint32_t finished_tick) {
  std::scoped_lock lock(this->mutex_);
  if (!p) return;
  if (p->finished_logged) return;   // needs a bool in Process
  p->finished_logged = true;
  if (contains(p->name())) {
    if (finished_by_name_[p->name()] == p)
      return; // Already inserted, skip duplicate
    rename_process(p);
  } else {
    duplicate_count_[p->name()] = 0;
  }

  time_t now = std::time(nullptr);
  finished_by_name_[p->name()] = p;
  finished_by_tick_.emplace(finished_tick, p);
  finished_by_time_.emplace(now, p);
}


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
    std::vector<std::tuple<uint32_t, ProcessPtr, time_t>> result;
    {
        
        result.reserve(finished_by_tick_.size());
        for (auto& [tick, weak] : finished_by_tick_) {
            if (auto proc = weak.lock()) {
                time_t t = 0;
                if (auto it = std::find_if(finished_by_time_.begin(),
                                           finished_by_time_.end(),
                                           [&](auto &pair){
                                               auto p2 = pair.second.lock();
                                               return p2 && p2 == proc;
                                           }); it != finished_by_time_.end()) {
                    t = it->first;
                }
                result.emplace_back(tick, proc, t);
            }
        }
    }
    // lock released here — safe to format or sort outside
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

    if (ordered_list.empty()) return "";

    oss << "Name\tFinished Time\tTick\tProgress\t#\n";
    oss << "------------------------------------------------------\n";

    for (const auto& [tick, proc, finish_time] : ordered_list) {
        if (counter >= 10) break; // <-- limit to top 10

        std::tm tm_buf{};
        localtime_r(&finish_time, &tm_buf);

        oss << proc->name() << "\t"
            << std::put_time(&tm_buf, "%d-%m-%Y %H:%M:%S") << "\t"
            << "(TICK " << tick << ")\t"
            << proc->get_executed_instructions() << " / "
            << proc->get_total_instructions() << "\t"
            << counter++ << "\n";
    }

    if (ordered_list.size() > 10)
        oss << "... (" << ordered_list.size() - 10 << " more)\n";

    return oss.str();
}


std::string FinishedMap::rename_process(std::shared_ptr<Process> process) {
    std::string name = process->name();

    // Base name without any suffix like _(1)
    std::string base_name = name;
    auto pos = name.find("_(");
    if (pos != std::string::npos)
        base_name = name.substr(0, pos);

    // Check how many times base_name has appeared
    uint32_t dupIndex = ++duplicate_count_[base_name];

    // If it's the first time, don't rename
    if (dupIndex == 1) {
        duplicate_count_[base_name] = 1;  // first seen
        return name;
    }

    // Otherwise, rename with incremented index
    std::ostringstream oss;
    oss << base_name << "_(" << (dupIndex - 1) << ")";
    std::string new_name = oss.str();

    process->set_name(new_name);
    return new_name;
}
