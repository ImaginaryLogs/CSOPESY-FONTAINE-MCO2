#include "kernel/scheduler.hpp"
#include "util.hpp"
// === Long-Term Scheduling API ===

void Scheduler::submit_process(std::shared_ptr<Process> p)
{
  p->set_state(ProcessState::NEW);
  this->job_queue_.send(p);
}

void Scheduler::long_term_admission()
{
  DEBUG_PRINT(DEBUG_SCHEDULER, "long_term_admission CHECK");

  while (!this->job_queue_.isEmpty()){
    auto p = this->job_queue_.receive();
    p->set_state(ProcessState::READY);
    this->ready_queue_.send(p);
  }
}
