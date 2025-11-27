#include "kernel/scheduler.hpp"
#include <iostream>

// === Short-Term Scheduling API ===

std::shared_ptr<Process> Scheduler::dispatch_to_cpu(uint32_t cpu_id)
{
    std::lock_guard<std::mutex> short_lock(short_term_mtx_);
    DEBUG_PRINT(DEBUG_CPU_WORKER, "%d is grabbing a process...", cpu_id);
    // If CPU already running a process, return it
    if (running_[cpu_id]) {
        if (!running_[cpu_id].get()) {
            std::cerr << "[WARN] running_[" << cpu_id << "] is null!\n";
            running_[cpu_id].reset();
            DEBUG_PRINT(DEBUG_CPU_WORKER, "%d is exiting without a process...", cpu_id);
            return nullptr;
        }
        return running_[cpu_id];
    }

    // Nothing to schedule
    if (this->ready_queue_.isEmpty()) {
      DEBUG_PRINT(DEBUG_CPU_WORKER, "%d is exiting without a process...", cpu_id);
        return nullptr;
    }

    // Fetch next process
    auto p = this->ready_queue_.receiveNext();
    if (!p) {
        //std::cerr << "[ERROR] Scheduler::dispatch_to_cpu received null process from ready_queue_\n";
        DEBUG_PRINT(DEBUG_CPU_WORKER, "%s id exiting without a process...", cpu_id);
        return nullptr;
    }

    for (const auto &r : running_) {
        if (r && r == p) {
            DEBUG_PRINT(DEBUG_CPU_WORKER, "%d is exiting without a process...", cpu_id);
            return nullptr;
        }
    }

    // Assign to CPU
    p->set_state(ProcessState::RUNNING);
    p->cpu_id = cpu_id;
    p->last_active_tick = this->tick_.load();
    running_[cpu_id] = p;

    if (cpu_quantum_remaining_.size() > cpu_id)
        cpu_quantum_remaining_[cpu_id] = this->cfg_.quantum_cycles - 1;
    DEBUG_PRINT(DEBUG_CPU_WORKER, "%d is executing a process...", cpu_id);
    return p;
}

void Scheduler::release_cpu_interrupt(uint32_t cpu_id, std::shared_ptr<Process> p, ProcessReturnContext context)
{

  if (!p) {
    std::cerr << "[ERROR] release_cpu_interrupt called with null process (cpu " << cpu_id << ")\n";
    return;
  }


  if (p->is_finished() || context.state == ProcessState::FINISHED){
    p->set_state(ProcessState::FINISHED);
    running_[cpu_id] = nullptr;
    finished_queue_.insert(p, tick_ + 1);

    {
        std::lock_guard<std::mutex> lock(scheduler_mtx_);
        process_map_.erase(p->id());
    }

    return;
  } else if (context.state == ProcessState::BLOCKED_PAGE_FAULT) {
      // Page Fault
      p->set_state(ProcessState::BLOCKED_PAGE_FAULT);
      running_[cpu_id] = nullptr;

      size_t page_num = p->get_faulting_page();

      handle_page_fault(p, page_num);
      return;

  } else if (p->is_waiting() || context.state == ProcessState::WAITING) {
    p->set_state(ProcessState::WAITING);
    running_[cpu_id] = nullptr;

    uint64_t duration = 0;
    try {
        if (!context.args.empty()) duration = std::stoull(context.args.at(0));
    } catch (...) { duration = 0; }

    uint64_t now = tick_.load();
    uint64_t wake_time = now + duration;
    sleep_queue_.send(p, wake_time);
    return;
  } else if (context.state == ProcessState::READY) {
        // Preemption: move back to ready queue
        p->set_state(ProcessState::READY);
        running_[cpu_id] = nullptr;

        // Use enqueue_ready to avoid duplicates
        enqueue_ready(p);
        return;
  } else if (context.state == ProcessState::RUNNING) {
    return;
  }

  p->set_state(ProcessState::READY);
  running_[cpu_id] = nullptr; // Fixed typo == to =
  enqueue_ready(p);
}

void Scheduler::short_term_dispatch(){
  DEBUG_PRINT(DEBUG_SCHEDULER, "short_term_dispatch CHECK");
  if (this->ready_queue_.isEmpty())
    return;

  for (uint32_t cpu_id = 0; cpu_id < this->cfg_.num_cpu; ++cpu_id){
    if (!running_[cpu_id]) dispatch_to_cpu(cpu_id);
  }
}
