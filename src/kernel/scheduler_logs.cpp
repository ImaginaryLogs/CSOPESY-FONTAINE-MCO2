#include "kernel/scheduler.hpp"
#include "data_structures/buffered_channel.hpp"
#include "data_structures/channel.hpp"
#include "data_structures/finished_map.hpp"
#include "data_structures/timer_entry.hpp"
#include "data_structures/dynamic_victim_channel.hpp"
#include "view/pager.hpp"
#include <thread>
#include <chrono>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <fstream>
#include <filesystem>
std::string Scheduler::snapshot() {
  std::ostringstream oss;
  std::ostringstream oss_mini;
  auto t = std::time(nullptr);
  auto tm = *std::localtime(&t);
  oss << "- CPU Snapshot ------------------------------------------------------------------------------\n";
  oss << "Paused: " << (paused_.load() ? "true" : "false")
      << " Tick: " << tick_.load()
      << " Algorithm: " << (cfg_.scheduler == SchedulingPolicy::FCFS ? "FCFS" : "RR")
      << std::put_time(&tm, "(%d-%m-%Y %H-%M-%S)") << "\n";
  // --- CPU States ---
  oss << "[CPU States]:\n";
  oss << cpu_state_snapshot();
  oss << "\n";
  oss << "----------------------------------------------t----------------------------------------------\n";
  oss_mini << "[Sleep Queue]\n"
      << (((sleep_queue_.isEmpty()))
        ? " (empty)\n\n"
        : sleep_queue_.snapshot());
  std::string sleep_string = oss_mini.str();
  oss_mini.str("");

  // --- Job Queue ---
  oss_mini << "[Job Queue]\n"
      << ((job_queue_.isEmpty())
        ? " (empty)\n\n"
        : job_queue_.snapshot());
  std::string job_string = oss_mini.str();
  oss_mini.str("");

  // --- Ready Queue ---
  oss_mini << "[Ready Queue]\n"
      << (ready_queue_.isEmpty()
        ? "  (empty)\n\n"
        : ready_queue_.snapshot());
  std::string ready_string = oss_mini.str();
  oss_mini.str("");

  // --- Finished ---
  std::string finished_snapshot = finished_queue_.snapshot();
  oss_mini  << "[Finished Processes]:\n"
            << ((finished_snapshot.empty())
            ? " (none)\n\n"
            : finished_snapshot);
  std::string finished_string = oss_mini.str();
  oss_mini.str("");

  oss << merge_columns(sleep_string, job_string, (size_t)45, " | ")
      << "\n----------------------------------------------+----------------------------------------------\n"
      << merge_columns(ready_string, finished_string, (size_t)45, " | ")
      << "\n----------------------------------------------^----------------------------------------------\n";
  
  return oss.str();
}

std::string Scheduler::snapshot_with_log() {
  save_snapshot("latest");
  return snapshot();
}

std::string Scheduler::get_sleep_queue_snapshot() {
    std::lock_guard<std::mutex> lock(scheduler_mtx_);
    return sleep_queue_.snapshot();
}


std::string Scheduler::cpu_state_snapshot() {
    std::vector<std::shared_ptr<Process>> procs;
    std::vector<uint32_t> quanta;

    procs = running_;
    quanta = cpu_quantum_remaining_;
     // unlock here before printing

    std::ostringstream oss;
    auto t = std::time(nullptr);
    auto tm = *std::localtime(&t);
    auto c = cpu_utilization();
    oss << "Used: " << c.used << "\n"
        << "Total: " << c.total << "\n"
        << "CPU UTIL:" << c.to_string() << "\n"
        << "\n"
        << "Core\tTime\t(Process Name)\t(Process ID)\t(Round Robin)\t(Last Active Time)\t(Executed Inst.)\t(Total Inst.)\n";
    for (size_t i = 0; i < procs.size(); ++i) {
        auto &proc = procs[i];
        if (proc) {
            oss << "Core: " << i << "\t"
                << std::put_time(&tm, "(%d-%m-%Y %H-%M-%S)") << "\t"
                << proc->name() << "\t"
                << "PID=" << proc->id() << "\t"
                << "RR=" << quanta[i] << "\t"
                << "LA=" << proc->last_active_tick << "\t"
                << proc->get_executed_instructions() << " / "
                << proc->get_total_instructions() << "\n";
        } else {
            oss << "  CPU " << i << ": IDLE\n";
        }
    }
    return oss.str();
}


void Scheduler::log_status(){
    if (this->tick_ % this->cfg_.snapshot_cooldown == 0)
        log_queue.send(Scheduler::snapshot());

    if (this->tick_ % this->cfg_.save_snapshot_file_rate == 0)
        save_snapshot("at_snapshot_rate");
        
    if (cfg_.remove_finished > 0 && finished_queue_.size() > cfg_.remove_finished_capacity )
        finished_queue_.clear();

}

void Scheduler::save_snapshot(std::string prefix = ""){
    std::filesystem::path log_path = "logs";
    std::filesystem::create_directories(log_path);
    std::ofstream sleep_file(std::format("{}/{}_sleep_queue.log", log_path.string(), prefix)),
                  ready_file(std::format("{}/{}_ready_queue.log", log_path.string(), prefix)),
                  job_file(std::format("{}/{}_job_queue.log", log_path.string(), prefix)),
                  finished_file(std::format("{}/{}_finished_queue.log", log_path.string(), prefix)),
                  running_file(std::format("{}/{}_running_cpu.log", log_path.string(), prefix));

    auto t = std::time(nullptr);
    auto tm = *std::localtime(&t);
    auto time = std::put_time(&tm, "TIME: (%d-%m-%Y %H-%M-%S)");
    sleep_file << time << "\n" 
               << sleep_queue_.print();
    ready_file << time << "\n" 
               << ready_queue_.print();
    job_file   << time << "\n" 
               << job_queue_.print();
    finished_file   << time << "\n" 
                    << finished_queue_.print();
    running_file    << time << "\n" 
                    << cpu_state_snapshot();
};
