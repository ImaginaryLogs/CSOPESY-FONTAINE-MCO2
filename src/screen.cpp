#include "../include/screen.hpp"

ScreenManager::ScreenManager() {}

bool ScreenManager::create_screen(const std::string &name,
                                  std::shared_ptr<Process> p) {
  std::lock_guard<std::mutex> lock(mtx_);
  if (screens_.find(name) != screens_.end()) {
    return false;
  }
  screens_[name] = p;
  return true;
}

std::shared_ptr<Process> ScreenManager::find(const std::string &name) {
  std::lock_guard<std::mutex> lock(mtx_);
  auto it = screens_.find(name);
  return it != screens_.end() ? it->second : nullptr;
}

std::string ScreenManager::list_summary() {
  std::lock_guard<std::mutex> lock(mtx_);
  // TODO: Implement proper summary
  return "";
}
