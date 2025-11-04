#include "../include/finished_map.hpp"
#include "../include/process.hpp"
#include <sstream>
#include <iostream>
#include <iomanip>
#include <ctime>

void FinishedMap::insert(ProcessPtr p, uint32_t finished_tick){
  std::scoped_lock<std::mutex> lock(this->mutex_);
  std::string name = p->name();

  if (finished_by_name_.count(name)){
    rename_process(p);
  } else {
    duplicate_count_[name] = 0;
  }
  auto t = std::time(nullptr);
  

  finished_by_name_[name] = p;
  finished_by_tick_.emplace(finished_tick, p);
  finished_by_time_.emplace(t, p);
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
    // auto get the paired finished_by_time_
    for (auto& [tick, weak] : finished_by_tick_) {
        if (auto proc = weak.lock())
            result.emplace_back(tick, proc, );
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
  
  for (const auto& pair : ordered_list){
    oss << pair.second->name() << "\t"
        << std::put_time(&tm, "%d-%m-%Y %H-%M-%S") << "\t"
        << "(TICK " << pair.first << ")\t"
        << pair.second->get_executed_instructions() << " / "
        << pair.second->get_total_instructions() << "\t"
        << counter << "\n";
    ++counter;
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