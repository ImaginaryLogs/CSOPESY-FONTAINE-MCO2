#pragma once
#include <map>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>
#include "processes/process.hpp"

using ProcessPtr = std::shared_ptr<Process>;
using OrderedEntry = std::pair<uint32_t, ProcessPtr>;

class FinishedMap {
  public:
    
    void insert(ProcessPtr p, uint32_t finished_tick);
    ProcessPtr get_by_name(const std::string& name);
    bool contains(const std::string& name);
    std::vector<std::tuple<uint32_t, ProcessPtr, time_t>> ordered();
    void clear();
    size_t size();
    std::string snapshot();

  private:
    std::string rename_process(ProcessPtr p);
    std::unordered_map<std::string, std::shared_ptr<Process>> finished_by_name_;
    std::multimap<uint32_t, std::weak_ptr<Process>, std::greater<>> finished_by_tick_;
    std::multimap<time_t, std::weak_ptr<Process>, std::greater<>> finished_by_time_;
    std::unordered_map<std::string, uint32_t> duplicate_count_;
    std::mutex mutex_;
};