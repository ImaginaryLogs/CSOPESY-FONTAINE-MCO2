#pragma once
#include "process.hpp"
#include <memory>
#include <mutex>
#include <unordered_map>

class ScreenManager {
public:
  ScreenManager();
  bool create_screen(const std::string &name, std::shared_ptr<Process> p);
  std::shared_ptr<Process> find(const std::string &name);
  std::string list_summary();
  // interactive attach will be implemented inside CLI using find() and
  // process->smi_summary()
private:
  std::mutex mtx_;
  std::unordered_map<std::string, std::shared_ptr<Process>> screens_;
};
