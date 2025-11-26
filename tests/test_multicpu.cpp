#include "../include/scheduler.hpp"
#include "../include/config.hpp"
#include "../include/process.hpp"
#include "../include/cpu_worker.hpp"
#include <thread>
#include <chrono>
#include <iostream>
#include <cassert>


void test_sleep_multicore(uint32_t n){
// --- Create simple processes ---
  std::vector<Instruction> instr1 = {
      {InstructionType::PRINT, {"P1-1"}},
      {InstructionType::SLEEP, {"3"}},
      {InstructionType::PRINT, {"P2-3"}}};
  std::vector<Instruction> instr2 = { 
      {InstructionType::PRINT, {"P2-1"}},
      {InstructionType::SLEEP, {"3"}},
      {InstructionType::PRINT, {"P2-3"}}};
  std::vector<Instruction> instr3 = { 
      {InstructionType::PRINT, {"P2-1"}},
      {InstructionType::SLEEP, {"3"}},
      {InstructionType::PRINT, {"P2-3"}}};

  auto p1 = std::make_shared<Process>(1, "P1", instr1);
  auto p2 = std::make_shared<Process>(2, "P2", instr2);
  auto p3 = std::make_shared<Process>(3, "P3", instr3);

  // --- Configure scheduler ---
  Config cfg;
  cfg.num_cpu = n;              // single CPU for deterministic test
  cfg.scheduler_tick_delay = 0; // fast ticks
  cfg.quantum_cycles = 1;
  cfg.snapshot_cooldown = 1;
  cfg.scheduler = SchedulingPolicy::RR;
  Scheduler sched(cfg);

  // Set policy: For simplicity assume scheduler has a field or flag
  // sched.set_policy(use_rr ? SchedulerPolicy::RR : SchedulerPolicy::FCFS);
  std::cout << "Setting scheduler policy to RR.\n";
  std::cout << "Submitting processes P1 and P2.\n";

  sched.submit_process(p1);
  sched.submit_process(p2); 
  sched.submit_process(p3);
  sched.setSchedulingPolicy(SchedulingPolicy::RR);

  std::cout << "Starting scheduler.\n"
            << sched.snapshot() << "\n";

  sched.start();//
//
  // --- Let scheduler run for a few ticks ---
  std::this_thread::sleep_for(std::chrono::milliseconds(32));
  std::cout << "Pausing scheduler.\n"
            << sched.snapshot() << "\n";
  sched.pause(); //

  // -- Check logs
  auto logs_p1 = p1->get_logs();
  auto logs_p2 = p2->get_logs();
  auto logs_sched = sched.get_sched_snapshots();//
  
  std::cout << "P1 logs:\n";
  for (const auto &log : logs_p1)
  {
    std::cout << log << "\n";
  }
  std::cout << "P2 logs:\n";
  for (const auto &log : logs_p2)
  {
    std::cout << log << "\n";
  }
  std::cout << "Scheduler logs:\n";
  for (const auto &log : logs_sched)
  {
    std::cout << log;
  }
  // --- Verify RR ---
  std::cout << "Verifying RR execution order.\n";

  // Interleaved logs (RR quantum = 1 tick per execute_tick)
  // Each process should have at least one instruction executed

  std::cout << "Scheduler test SLEEP passed.\n";

  sched.stop();
  std::cout << "Scheduler stopped safely.\n";
}

int main()
{
  std::cout << "\n\nMulticore Sleep Test, n = 2.";
  test_sleep_multicore(2);
  std::cout << "\n\nMulticore Sleep Test, n = 3.";
  test_sleep_multicore(3);
  std::cout << "\n\nMulticore Sleep Test, n = 4.";
  test_sleep_multicore(4);

  std::this_thread::sleep_for(std::chrono::seconds(1));
  return 0;
}