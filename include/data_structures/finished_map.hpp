#pragma once
#include <map>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>
#include "processes/process.hpp"

using ProcessPtr = std::shared_ptr<Process>;
using OrderedFinishedEntry = std::pair<uint32_t, ProcessPtr>;

class FinishedMap {
  public:
    void insert(ProcessPtr p, uint32_t finished_tick);
    void clear();
    size_t size();
    std::string snapshot();
    std::string print();

  private:
    std::multimap<time_t, std::shared_ptr<OrderedFinishedEntry>, std::greater<>> finished_by_time_;
    std::mutex mutex_;
};