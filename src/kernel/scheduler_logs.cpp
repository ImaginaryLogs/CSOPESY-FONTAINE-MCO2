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

std::string Scheduler::snapshot() {
  std::ostringstream oss;
  std::ostringstream oss_mini;
  oss << "─ CPU Snapshot ──────────────────────────────────────────────────────────────────────────────\n";
  oss << "Paused: " << (paused_.load() ? "true" : "false")
      << " Tick: " << tick_.load()
      << " Algorithm: " << (cfg_.scheduler == SchedulingPolicy::FCFS ? "FCFS" : "RR") << "\n";
  // --- CPU States ---
  oss << "[CPU States]:\n";
  oss << cpu_state_snapshot();
  oss << "\n";
  oss << "──────────────────────────────────────────────┬──────────────────────────────────────────────\n";
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

  oss << merge_columns(sleep_string, job_string, (size_t)45, " │ ");
  oss << "\n──────────────────────────────────────────────┼──────────────────────────────────────────────\n";
  oss << merge_columns(ready_string, finished_string, (size_t)45, " │ ");

  oss << "\n──────────────────────────────────────────────┴──────────────────────────────────────────────\n";

  return oss.str();
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

    for (size_t i = 0; i < procs.size(); ++i) {
        auto &proc = procs[i];
        if (proc) {
            oss << proc->name() << "\t"
                << std::put_time(&tm, "%d-%m-%Y %H-%M-%S") << "\t"
                << "PID=" << proc->id() << "\t"
                << "RR=" << quanta[i] << "\t"
                << "LA=" << proc->last_active_tick << "\t"
                << "Core: " << i << "\t"
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
        save_snapshot();
    if (cfg_.remove_finished > 0 && finished_queue_.size() > cfg_.remove_finished_capacity )
        finished_queue_.clear();

}

void Scheduler::save_snapshot(){
    std::ofstream sleep_file("s_sleep_queue.log"),
                  ready_file("s_ready_queue.log"),
                  job_file("s_job_queue.log"),
                  finished_file("s_finished_queue.log"),
                  running_file("s_running_cpu.log");
    sleep_file << sleep_queue_.print();
    ready_file << ready_queue_.print();
    job_file << job_queue_.print();
    finished_file << finished_queue_.print();
    running_file << cpu_state_snapshot();
};
