#include "../include/cpu_worker.hpp"
#include "../include/scheduler.hpp"
#include <iostream>

#define DEBUG_CPU_WORKER true

/**
 * NOTE:
 * This is just a skeleton that CJ created to get things going.
 * Feel free to add/remove/revise anything.
 */

CPUWorker::CPUWorker(uint32_t id, Scheduler &sched) : id_(id), sched_(sched) {}

void CPUWorker::start() {
  if (running_.load())
    return;
  #if DEBUG_CPU_WORKER
  std::cout << "CPUWorker " << id_ << " starting.\n";
  #endif

  running_.store(true);
  thread_ = std::thread(&CPUWorker::loop, this);

}

void CPUWorker::stop() { 
  #if DEBUG_CPU_WORKER
  std::cout << "CPUWorker " << id_ << " stopping.\n";
  #endif
  running_.store(false); 
  sched_.stop_barrier_sync();
}


void CPUWorker::join() {
  if (thread_.joinable()) {
    thread_.join();
  }
}

void CPUWorker::loop() {
  #if DEBUG_CPU_WORKER
  std::cout << "CPUWorker " << id_ << " entering main loop.\n";
  #endif

  while (running_.load()) {

    while (sched_.is_paused()) {
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    #if DEBUG_CPU_WORKER
    std::cout<< "CPUWorker " << id_ << " waiting for pre-emption to execute.\n";
    #endif
    sched_.tick_barrier_sync();
    #if DEBUG_CPU_WORKER
    std::cout << "CPUWorker " << id_ << " checking for process to execute.\n";
    #endif
    
    auto process = sched_.dispatch_to_cpu(this->id_);
    uint32_t consumed_ticks = 1; // max ticks possible in one execute_tick call

    if (!process) {
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
      #if DEBUG_CPU_WORKER
      std::cout << "CPUWorker " << id_ << " idle for this tick.\n";
      #endif
      sched_.tick_barrier_sync();
      continue;
    } else {
      std::cout << "CPUWorker " << id_ << " found process ID=" << process->id() << " with PR=" << process->priority << ", LA=" << process->last_active_tick << "\n";
    }
    
    bool finished = process->execute_tick(sched_.current_tick(), sched_.get_scheduler_tick_delay(), consumed_ticks);
    
    if (finished) {
      sched_.release_cpu(this->id_, process, true, false);
    } 

  #if DEBUG_CPU_WORKER
  std::cout << "CPUWorker " << id_ << " executed tick for Process " << process->id()
        << ", consumed ticks: " << consumed_ticks << "\n";
  #endif
    sched_.tick_barrier_sync();
  }
}
