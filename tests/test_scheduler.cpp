#include "../include/scheduler.hpp"
#include "../include/config.hpp"
#include "../include/process.hpp"
#include "../include/cpu_worker.hpp"
#include <thread>
#include <chrono>
#include <iostream>
#include <cassert>

void test_scheduler_algo(bool use_rr)
{
  // --- Create simple processes ---
  std::vector<Instruction> instr1 = {
      {InstructionType::PRINT, {"P1-1"}},
      {InstructionType::PRINT, {"P1-2"}},
      {InstructionType::PRINT, {"P2-3"}}};
  std::vector<Instruction> instr2 = { 
      {InstructionType::PRINT, {"P2-1"}},
      {InstructionType::PRINT, {"P2-2"}},
      {InstructionType::PRINT, {"P2-3"}}};
  std::vector<Instruction> instr3 = { 
      {InstructionType::PRINT, {"P2-1"}},
      {InstructionType::PRINT, {"P2-2"}},
      {InstructionType::PRINT, {"P2-3"}}};

  auto p1 = std::make_shared<Process>(1, "P1", instr1);
  auto p2 = std::make_shared<Process>(2, "P2", instr2);
  auto p3 = std::make_shared<Process>(3, "P3", instr3);

  // --- Configure scheduler ---
  Config cfg;
  cfg.num_cpu = 1;              // single CPU for deterministic test
  cfg.scheduler_tick_delay = 1; // fast ticks
  cfg.quantum_cycles = 1;
  cfg.snapshot_cooldown = 1;
  cfg.scheduler = use_rr ? SchedulingPolicy::RR : SchedulingPolicy::FCFS;
  Scheduler sched(cfg);

  // Set policy: For simplicity assume scheduler has a field or flag
  // sched.set_policy(use_rr ? SchedulerPolicy::RR : SchedulerPolicy::FCFS);
  std::cout << "Setting scheduler policy to " << (use_rr ? "RR" : "FCFS") << ".\n";
  std::cout << "Submitting processes P1 and P2.\n";

  sched.submit_process(p1);
  sched.submit_process(p2);
  sched.submit_process(p3);
  sched.setSchedulingPolicy(use_rr ? SchedulingPolicy::RR : SchedulingPolicy::FCFS);

  std::cout << "Starting scheduler.\n"
            << sched.snapshot() << "\n";

  sched.start();//

  // --- Let scheduler run for a few ticks ---
  std::this_thread::sleep_for(std::chrono::milliseconds(20));
  std::cout << "Pausing scheduler.\n"
            << sched.snapshot() << "\n";
  sched.pause(); //

  // -- Check logs
  auto logs_p1 = p1->get_logs();
  auto logs_p2 = p2->get_logs();
  auto logs_sched = sched.get_sched_snapshots();
  
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

  // --- Verify FCFS ---
  if (!use_rr)
  {
    // P1 should run fully before P2 starts
    std::cout << "Verifying FCFS execution order.\n";
    
    assert(logs_p1.size() >= 2);
    assert(logs_p1[0] == "P1-1");
    assert(logs_p1[1] == "P1-2");

    assert(logs_p2.size() >= 2);
    assert(logs_p2[0] == "P2-1");
    assert(logs_p2[1] == "P2-2");
  }
  else
  {
    // --- Verify RR ---
    std::cout << "Verifying RR execution order.\n";

    // Interleaved logs (RR quantum = 1 tick per execute_tick)
    // Each process should have at least one instruction executed
    assert(logs_p1.size() >= 1);
    assert(logs_p2.size() >= 1);

    // First tick should be p1
    assert(logs_p1[0] == "P1-1");
    assert(logs_p2[0] == "P2-1");


  }

  std::cout << "Scheduler test (" << (use_rr ? "RR" : "FCFS") << ") passed.\n";

  sched.stop();
  std::cout << "Scheduler stopped safely.\n";
}

void test_resume()
{
  Config cfg;
  cfg.num_cpu = 2;
  cfg.scheduler_tick_delay = 100;

  Scheduler sched(cfg);
  sched.start();
  std::cout << "Starting." << "\n";
  std::cout << sched.snapshot() << "\n";
  std::this_thread::sleep_for(std::chrono::seconds(2));

  sched.pause();
  std::cout << "Paused at tick: " << sched.current_tick() << "\n";
  std::cout << sched.snapshot() << "\n";
  std::this_thread::sleep_for(std::chrono::seconds(2));

  sched.resume();
  std::cout << "Resumed.\n";
  std::cout << sched.snapshot() << "\n";
  std::this_thread::sleep_for(std::chrono::seconds(2));
  std::cout << "Stopping scheduler...\n";

  sched.stop();
  std::cout << "Stopped safely.\n";
}

int main()
{
  // --- Test pause/resume ---

  // test_resume();
  // std::this_thread::sleep_for(std::chrono::seconds(2));

  // --- Test FCFS ---
  test_scheduler_algo(false);
  std::this_thread::sleep_for(std::chrono::seconds(2));

  // --- Test Round-Robin ---
  test_scheduler_algo(true);
  std::this_thread::sleep_for(std::chrono::seconds(2));

  return 0;
}
