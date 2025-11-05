#include "../include/cpu_worker.hpp"
#include "../include/process.hpp"
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
  std::cout << "CPU Worker " << id_ << " starting.\n";
#endif

  running_.store(true);
  thread_ = std::thread(&CPUWorker::loop, this);
}

void CPUWorker::stop() {
#if DEBUG_CPU_WORKER
  std::cout << "CPU Worker " << id_ << " stopping.\n";
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
  while (running_.load()) {

    while (sched_.is_paused()) std::this_thread::sleep_for(std::chrono::milliseconds(1));

    sched_.tick_barrier_sync();

    auto process = sched_.dispatch_to_cpu(this->id_);

    uint32_t consumed_ticks = 2; // max ticks possible in one execute_tick call

    // #if DEBUG_CPU_WORKER
    // std::cout << "CPU Worker executing for CORE " << this->id_ << " at tick \n";
    // #endif

    if (!process) {
      sched_.tick_barrier_sync();
      sched_.tick_barrier_sync();
      sched_.tick_barrier_sync();
      continue;
    }
    
    ProcessReturnContext context = process->execute_tick(
        sched_.current_tick(), 
        sched_.get_scheduler_tick_delay(),
        consumed_ticks);
    //std::cout << "Tick "<< sched_.current_tick() << " inst: " << process->get_executed_instructions() << " out of " << process->get_total_instructions() << "\n";
    if (is_yielded(context)) sched_.release_cpu_interrupt(this->id_, process, context);

    sched_.tick_barrier_sync();
    sched_.tick_barrier_sync(); // Here, scheduler increases timer. Second tick barrier is essential
  }
}
