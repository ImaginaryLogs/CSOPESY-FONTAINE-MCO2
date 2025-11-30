#include "kernel/cpu_worker.hpp"
#include "processes/process.hpp"
#include "kernel/scheduler.hpp"
#include "util.hpp"
#include <iostream>
#include <syncstream>
#include <fstream>



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

void CPUWorker::detach() {
  if (thread_.joinable()) thread_.detach();
}

void CPUWorker::loop() {
  try {
    std::string id_str = "CPU " + std::to_string((int)this->id_);
    const char * id = id_str.c_str();
    while (running_.load()) {

    while (sched_.is_paused()) {
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    sched_.tick_barrier_sync(id_str, 1);
    DEBUG_PRINT(DEBUG_CPU_WORKER, "%s grabs a process from cpu", id);
    auto process = sched_.dispatch_to_cpu(this->id_);

    uint32_t consumed_ticks = 1; // max ticks possible in one execute_tick call

    if (!process) {
      DEBUG_PRINT(DEBUG_CPU_WORKER, "%s has none at the moment.", id);
      // Account idle tick
      sched_.account_cpu_idle(this->id_);
      sched_.tick_barrier_sync(id_str, 2);
      sched_.tick_barrier_sync(id_str, 3);
      continue;
    }
    DEBUG_PRINT(DEBUG_CPU_WORKER, "%s executing a process", id);

    ProcessReturnContext context = process->execute_tick(
        sched_.current_tick(),
        sched_.get_scheduler_tick_delay(),
        consumed_ticks);
    std::string state = process->get_state_string();
    DEBUG_PRINT(DEBUG_CPU_WORKER, "%s executed %u with status %s\n", id, process->get_executed_instructions(), state.c_str());

    if (is_yielded(context)) sched_.release_cpu_interrupt(this->id_, process, context);

    // Account busy tick (we executed work this cycle)
    sched_.account_cpu_busy(this->id_);

    sched_.tick_barrier_sync(id_str, 2); // Here, scheduler increases timer. Second tick barrier is essential
    sched_.tick_barrier_sync(id_str, 3);
    
    // CRITICAL: Sleep to match scheduler tick rate!
    // Without this, workers loop faster than scheduler and hit barrier 1 again
    // while scheduler is still sleeping, causing desync and deadlock
    auto delay = sched_.get_scheduler_tick_delay();
    std::this_thread::sleep_for(std::chrono::milliseconds(delay));
    }
  } catch (const std::exception &ex) {
    std::ofstream f("logs/crash.log", std::ios::app);
    f << "CPUWorker exception: " << ex.what() << "\n";
    f.close();
  } catch (...) {
    std::ofstream f("logs/crash.log", std::ios::app);
    f << "CPUWorker unknown exception\n";
    f.close();
  }
}
