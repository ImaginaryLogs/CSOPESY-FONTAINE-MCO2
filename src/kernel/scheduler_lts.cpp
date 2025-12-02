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
    size_t mem_size = p->get_memory_requirement();
    if (mem_size == 0) {
      mem_size = cfg_.min_mem_per_proc;
      if (cfg_.max_mem_per_proc > cfg_.min_mem_per_proc) {
        mem_size += (rand() % (cfg_.max_mem_per_proc - cfg_.min_mem_per_proc + 1));
      }
    }
    // Align to frame size
    if (mem_size % cfg_.mem_per_frame != 0) {
      mem_size = ((mem_size / cfg_.mem_per_frame) + 1) * cfg_.mem_per_frame;
    }

    p->initialize_memory(mem_size, cfg_.mem_per_frame);

    // EAGER ALLOCATION: Pre-allocate all pages for the process
    // This will cause deadlock if total memory requirement exceeds available RAM
    size_t num_pages = mem_size / cfg_.mem_per_frame;
    for (size_t page = 0; page < num_pages; ++page) {
      auto res = MemoryManager::getInstance().request_page(p->id(), page, false);
      p->update_page_table(page, res.frame_idx);
      
      // Handle eviction if needed
      if (res.evicted_page) {
        uint32_t victim_pid = res.evicted_page->first;
        size_t victim_page = res.evicted_page->second;
        auto victim = get_process(victim_pid);
        if (victim) {
          victim->invalidate_page(victim_page);
        }
      }
    }

    // Add to process map
    process_map_[p->id()] = p;

    DEBUG_PRINT(DEBUG_SCHEDULER, "Admitting process %d with %zu bytes memory (%zu pages)", p->id(), mem_size, num_pages);

    p->set_state(ProcessState::READY);
    ready_queue_.send(p);
  }
}
