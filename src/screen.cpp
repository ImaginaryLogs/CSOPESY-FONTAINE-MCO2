#include "../include/screen.hpp"
#include "../include/reporter.hpp"
#include "../include/scheduler.hpp"
#include <sstream>

ScreenManager::ScreenManager() = default;

// Create a new screen entry if it doesn't already exist
bool ScreenManager::create_screen(const std::string &name, std::shared_ptr<Process> p) {
  
  std::lock_guard<std::mutex> lock(mtx_);

  if (screens_.contains(name))
    return false;

  screens_[name] = std::move(p);

  return true;
}

// Find an existing screen by name
std::shared_ptr<Process> ScreenManager::find(const std::string &name) {

  std::lock_guard<std::mutex> lock(mtx_);
  auto it = screens_.find(name);

  return (it != screens_.end()) ? it->second : nullptr;
}

// Placeholder, listing handled by reporter via Scheduler::snapshot()
std::string ScreenManager::list_summary() {
  return {};
}
