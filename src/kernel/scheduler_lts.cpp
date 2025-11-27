#include "kernel/scheduler.hpp"
#include "util.hpp"
#include "paging/memory_manager.hpp"
#include <cstdlib>  // for rand()

// === Long-Term Scheduling API ===

void Scheduler::submit_process(std::shared_ptr<Process> p)
{
  p->set_state(ProcessState::NEW);
  this->job_queue_.send(p);
}

void Scheduler::long_term_admission()
{
  DEBUG_PRINT(DEBUG_SCHEDULER, "long_term_admission CHECK");

  // 1. Check job queue
  while (!job_queue_.isEmpty()) {
    auto p = job_queue_.receive();
    if (!p) continue;

    // Initialize memory
    // Calculate memory limit based on config
    // "M is the rolled value between min-mem-per-proc and max-mem-proc"
    // Specs: "M is the rolled value". Implies random.
    size_t mem_size = cfg_.min_mem_per_proc;
    if (cfg_.max_mem_per_proc > cfg_.min_mem_per_proc) {
      mem_size += (rand() % (cfg_.max_mem_per_proc - cfg_.min_mem_per_proc + 1));
    }
    // Align to frame size
    if (mem_size % cfg_.mem_per_frame != 0) {
      mem_size = ((mem_size / cfg_.mem_per_frame) + 1) * cfg_.mem_per_frame;
    }

    p->initialize_memory(mem_size, cfg_.mem_per_frame);

    // Add to process map
    process_map_[p->id()] = p;

    DEBUG_PRINT(DEBUG_SCHEDULER, "Admitting process %d with %zu bytes memory", p->id(), mem_size);

    p->set_state(ProcessState::READY);
    ready_queue_.send(p);
  }
}
