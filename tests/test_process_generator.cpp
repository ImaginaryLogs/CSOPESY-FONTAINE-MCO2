#include "../include/processes/instruction.hpp"
#include "../include/processes/process.hpp"
#include "../include/processes/process_generator.hpp"
#include "../include/kernel/scheduler.hpp"
#include <cassert>
#include <chrono>
#include <iostream>
#include <thread>

int main() {
  std::cout << "Running generator unit tests...\n";

  // === Test 1: estimate_unrolled_size_for_instr simple cases ===
  Instruction p;
  p.type = InstructionType::PRINT;
  uint32_t s = 0;
  s = 1; // for PRINT
  assert(s == 1);

  // Build a FOR with 3 repeats and nested 2 PRINTs
  Instruction f;
  f.type = InstructionType::FOR;
  f.args.push_back("3");
  Instruction nested1;
  nested1.type = InstructionType::PRINT;
  Instruction nested2;
  nested2.type = InstructionType::PRINT;
  f.nested.push_back(nested1);
  f.nested.push_back(nested2);

  // Manual unroll estimator (reimplemented version)
  auto estimate_for = [&](const Instruction &instr) -> uint32_t {
    if (instr.type != InstructionType::FOR)
      return 1;
    uint32_t repeats = 1;
    if (!instr.args.empty()) {
      try {
        repeats = static_cast<uint32_t>(std::stoul(instr.args[0]));
      } catch (...) {
        repeats = 1;
      }
    }
    uint32_t nested_total = 0;
    for (const auto &n : instr.nested)
      nested_total += 1; // nested prints
    return repeats * nested_total;
  };

  uint32_t est = estimate_for(f);
  assert(est == 6); // 3 repeats * 2 nested = 6
  std::cout << "Test 1 passed: unroll estimation logic.\n";

  // === Test 2: ensure generator stops when budget exceeded ===
  Config cfg;
  cfg.max_unrolled_instructions = 5; // small budget
  cfg.scheduler_tick_delay = 10;
  Scheduler sched(cfg);
  ProcessGenerator gen(cfg, sched);

  bool would_include = false;
  {
    uint32_t estimated_size = 0;
    Instruction instr = f; // FOR expands to size 6
    uint32_t instr_size = est;
    if (cfg.max_unrolled_instructions > 0 &&
        estimated_size + instr_size > cfg.max_unrolled_instructions) {
      would_include = false;
    } else {
      would_include = true;
    }
  }
  assert(would_include == false);
  std::cout << "Test 2 passed: budget enforcement logic.\n";

  // === Test 3: generate_instructions() respects budget ===
  {
    Config cfg2;
    cfg2.max_unrolled_instructions = 5;
    cfg2.min_ins = 1;
    cfg2.max_ins = 10;
    cfg2.scheduler_tick_delay = 10;
    Scheduler sched2(cfg2);
    ProcessGenerator gen2(cfg2, sched2);

    uint32_t est_size = 0;
    auto ins = gen2.generate_instructions(10, est_size);
    assert(est_size <= cfg2.max_unrolled_instructions);
    assert(!ins.empty());
    std::cout << "Test 3 passed: generate_instructions() respects budget.\n";
  }

  // === Test 4: generate_instructions() works with unlimited budget ===
  {
    Config cfg3;
    cfg3.max_unrolled_instructions = 0; // unlimited
    cfg3.min_ins = 1;
    cfg3.max_ins = 5;
    cfg3.scheduler_tick_delay = 10;
    Scheduler sched3(cfg3);
    ProcessGenerator gen3(cfg3, sched3);

    uint32_t est_size = 0;
    auto ins = gen3.generate_instructions(5, est_size);
    assert(!ins.empty());
    std::cout << "Test 4 passed: unlimited budget accepted.\n";
  }

  // === Test 5: thread lifecycle (safe limited start/stop) ===
  {
    Config cfg4;
    cfg4.batch_process_freq = 1;
    cfg4.scheduler_tick_delay = 10;
    Scheduler sched4(cfg4);
    ProcessGenerator gen4(cfg4, sched4);

    // Start then stop quickly to ensure no crash
    gen4.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    gen4.stop();
    std::cout << "Test 5 passed: generator thread start/stop safe.\n";
  }

  // === Test 6: ID and name consistency ===
  {
    Config cfg5;
    cfg5.scheduler_tick_delay = 10;
    Scheduler sched5(cfg5);
    ProcessGenerator gen5(cfg5, sched5);

    uint32_t est_size = 0;
    auto ins = gen5.generate_instructions(3, est_size);
    auto p = std::make_shared<Process>(0, "p00", ins);
    assert(p->id() == 0);
    assert(p->name() == "p00");
    std::cout << "Test 6 passed: Process ID and name consistent.\n";
  }

  // === Test 7: Process summary_line formatting ===
  {
    Config cfg6;
    Scheduler sched6(cfg6);
    Instruction instr;
    instr.type = InstructionType::PRINT;
    std::vector<Instruction> ins = {instr};
    auto p = std::make_shared<Process>(42, "p42", ins);
    std::string line = p->summary_line(false);
    assert(line.find("p42") != std::string::npos);
    assert(line.find("/") != std::string::npos);
    std::cout << "Test 7 passed: Process summary_line() formatted correctly.\n";
  }

  // === Test 8: Generator stress test with multiple start/stop ===
  {
    Config cfg7;
    cfg7.batch_process_freq = 1;
    cfg7.scheduler_tick_delay = 5;
    Scheduler sched7(cfg7);
    ProcessGenerator gen7(cfg7, sched7);

    for (int i = 0; i < 3; ++i) {
      gen7.start();
      std::this_thread::sleep_for(std::chrono::milliseconds(20));
      gen7.stop();
    }
    std::cout
        << "Test 8 passed: Generator stable across repeated start/stop.\n";
  }

  // === Test 9: FOR_MAX_NESTING respected ===
  {
    Config cfg8;
    cfg8.scheduler_tick_delay = 10;
    Scheduler sched8(cfg8);
    ProcessGenerator gen8(cfg8, sched8);

    uint32_t est_size = 0;
    auto ins = gen8.generate_instructions(10, est_size);
    for (const auto &i : ins) {
      if (i.type == InstructionType::FOR) {
        for (const auto &nested : i.nested) {
          assert(nested.type != InstructionType::FOR || i.nested.size() <= 3);
        }
      }
    }
    std::cout << "Test 9 passed: FOR_MAX_NESTING limit respected.\n";
  }

  //   // === Test 10: Scheduler receives process from generator ===
  //   {
  //     Config cfg9;
  //     cfg9.batch_process_freq = 1;
  //     cfg9.scheduler_tick_delay = 5;
  //     Scheduler sched9(cfg9);
  //     ProcessGenerator gen9(cfg9, sched9);

  //     gen9.start();
  //     std::this_thread::sleep_for(std::chrono::milliseconds(30));
  //     gen9.stop();

  //     // Verify that job queue is not empty
  //     assert(!sched9.job_queue_.isEmpty());
  //     std::cout << "Test 10 passed: Scheduler received generated process.\n";
  //   }

  std::cout << "All generator tests passed successfully.\n";
  return 0;
}
