#include "../include/finished_map.hpp"
#include <sstream>
#include <iomanip>
#include <ctime>

void FinishedMap::insert(ProcessPtr p, uint32_t finished_tick){
  std::scoped_lock<std::mutex> lock(mutex_);
  // ensure unique name if duplicates
  rename_process(p);
  finished_by_name_[p->name()] = p;
  finished_by_tick_.emplace(finished_tick, p);
  finished_by_time_.emplace(std::time(nullptr), p);
}

ProcessPtr FinishedMap::get_by_name(const std::string& name){
  std::scoped_lock<std::mutex> lock(mutex_);
  auto it = finished_by_name_.find(name);
  return (it==finished_by_name_.end()) ? nullptr : it->second;
}

bool FinishedMap::contains(const std::string& name){
  std::scoped_lock<std::mutex> lock(mutex_);
  return finished_by_name_.count(name) != 0;
}

std::vector<OrderedEntry> FinishedMap::ordered(){
  std::scoped_lock<std::mutex> lock(mutex_);
  std::vector<OrderedEntry> out;
  out.reserve(finished_by_tick_.size());
  for (auto &kv : finished_by_tick_){
    if (auto sp = kv.second.lock()){
      out.emplace_back(kv.first, sp);
    }
  }
  return out;
}

void FinishedMap::clear(){
  std::scoped_lock<std::mutex> lock(mutex_);
  finished_by_name_.clear();
  finished_by_tick_.clear();
  finished_by_time_.clear();
  duplicate_count_.clear();
}

size_t FinishedMap::size(){
  std::scoped_lock<std::mutex> lock(mutex_);
  return finished_by_name_.size();
}

std::string FinishedMap::snapshot(){
  std::scoped_lock<std::mutex> lock(mutex_);
  std::ostringstream oss;
  if (finished_by_tick_.empty()) return {};
  for (auto &kv : finished_by_tick_){
    auto p = kv.second.lock();
    if (!p) continue;
    oss << "  " << p->name() << "   Finished   "
        << p->get_executed_instructions() << " / "
        << p->get_total_instructions() << "\n";
  }
  return oss.str();
}

std::string FinishedMap::rename_process(ProcessPtr p){
  std::string name = p->name();
  if (finished_by_name_.count(name)){
    uint32_t idx = ++duplicate_count_[name];
    std::ostringstream os; os<<name<<"_("<<idx<<")";
    p->set_name(os.str());
    return p->name();
  }
  duplicate_count_[name] = 0;
  return name;
}
