#include "kernel/scheduler.hpp"
#include <thread>
#include <chrono>
#include <iostream>
std::string Scheduler::snapshot() {
  std::ostringstream oss;
  oss << "=== Scheduler Snapshot ===| ---\n";
  oss << "Tick: " << tick_.load() << "\n";
  oss << "Paused: " << (paused_.load() ? "true" : "false") << "\n";

  oss << "[Sleep Queue]\n"
      << (((sleep_queue_.isEmpty()))
        ? " (empty)\n\n"
        : sleep_queue_.print_sleep_queue());
  
  // --- Job Queue ---
  oss << "[Job Queue]\n"
      << ((job_queue_.isEmpty())
        ? " (empty)\n\n" 
        : job_queue_.snapshot());

  // --- Ready Queue ---
  oss << "[Ready Queue]\n";
  oss << (ready_queue_.isEmpty() 
        ? "  (empty)\n\n" 
        : ready_queue_.snapshot());
  
  // --- CPU States ---
  oss << "\n[CPU States]:\n";
  oss << cpu_state_snapshot();
  oss << "\n";  

  // --- Finished ---
  oss << "[Finished Processes]:\n";
  std::string finished_snapshot = finished_.snapshot();
  oss << ((finished_snapshot.empty()) 
        ? " (none)\n\n"
        : finished_snapshot);

  oss << "===========================\n";

  return oss.str();
}

std::string Scheduler::get_sleep_queue_snapshot() {
    std::lock_guard<std::mutex> lock(scheduler_mtx_);
    return sleep_queue_.print_sleep_queue();
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
}