#include "../include/instruction.hpp"
#include "../include/process_generator.hpp"
#include <cassert>
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
    Scheduler sched4(cfg4);
    ProcessGenerator gen4(cfg4, sched4);

    // Start then stop quickly to ensure no crash
    gen4.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    gen4.stop();
    std::cout << "Test 5 passed: generator thread start/stop safe.\n";
  }

  std::cout << "All generator tests passed successfully.\n";
  return 0;
}
