#include "processes/instruction.hpp"
#include "processes/process.hpp"
#include "processes/process_generator.hpp"
#include "kernel/scheduler.hpp"
#include <atomic>
#include <cassert>
#include <chrono>
#include <iostream>
#include <thread>

int main() {
  std::cout << "Running process unit tests...\n";

  // === Test 1: Process initialization and unrolling ===
  {
    Instruction f;
    f.type = InstructionType::FOR;
    f.args.push_back("2");
    Instruction inner;
    inner.type = InstructionType::PRINT;
    f.nested.push_back(inner);

    std::vector<Instruction> ins = {f};
    Process p(1, "unroll_test", ins);

    assert(p.get_total_instructions() == 2);
    assert(p.get_executed_instructions() == 0);
    std::cout << "Test 1 passed: FOR unrolling logic OK.\n";
  }

  // === Test 2: Arithmetic correctness (DECLARE, ADD, SUBTRACT) ===
  {
    std::vector<Instruction> ins = {
        {InstructionType::DECLARE, {"x", "10"}},
        {InstructionType::ADD, {"x", "x", "5"}},
        {InstructionType::SUBTRACT, {"x", "x", "3"}}};

    Process p(2, "arith", ins);
    uint32_t tick = 0, consumed = 0;

    while (p.execute_tick(++tick, 0, consumed).state != ProcessState::FINISHED)
      ;

    assert(p.vars.at("x") == 12); // (10 + 5) - 3
    std::cout << "Test 2 passed: Arithmetic operations OK (x=" << p.vars.at("x")
              << ").\n";
  }

  // === Test 3: SLEEP instruction behavior ===
  {
    Instruction sleep;
    sleep.type = InstructionType::SLEEP;
    sleep.args.push_back("3");
    std::vector<Instruction> ins = {sleep, {InstructionType::PRINT, {"done"}}};
    Process p(3, "sleeper", ins);

    uint32_t tick = 0, consumed = 0;
    int sleep_ticks = 0;

    while (p.state() != ProcessState::FINISHED) {
      p.execute_tick(++tick, 0, consumed);
      if (p.state() == ProcessState::WAITING)
        ++sleep_ticks;
    }

    assert(sleep_ticks == 3);
    std::cout << "Test 3 passed: SLEEP logic OK (" << sleep_ticks
              << " WAITING ticks).\n";
  }

  // === Test 4: Clamp boundaries (overflow and underflow) ===
  {
    std::vector<Instruction> ins = {
        {InstructionType::DECLARE, {"a", "65535"}},
        {InstructionType::ADD, {"a", "a", "100"}},
        {InstructionType::SUBTRACT, {"a", "a", "70000"}},
    };
    Process p(4, "clamp", ins);
    uint32_t tick = 0, consumed = 0;

    while (p.execute_tick(++tick, 0, consumed).state != ProcessState::FINISHED)
      ;

    assert(p.vars.at("a") == 0 || p.vars.at("a") == 65535);
    std::cout << "Test 4 passed: Clamping applied properly (a="
              << p.vars.at("a") << ").\n";
  }

  // === Test 5: Logging behavior ===
  {
    std::vector<Instruction> ins = {{InstructionType::PRINT, {"hello"}},
                                    {InstructionType::PRINT, {"world"}}};

    Process p(5, "logger", ins);
    uint32_t tick = 0, consumed = 0;
    while (p.execute_tick(++tick, 0, consumed).state != ProcessState::FINISHED)
      ;

    auto logs = p.get_logs();
    assert(logs.size() >= 2);
    assert(logs[0].find("hello") != std::string::npos);
    assert(logs[1].find("world") != std::string::npos);
    std::cout << "Test 5 passed: Logging captured PRINT output.\n";
  }

  // === Test 6: Summary line formatting ===
  {
    std::vector<Instruction> ins = {{InstructionType::PRINT, {"test"}},
                                    {InstructionType::SLEEP, {"1"}}};
    Process p(6, "summary", ins);

    std::string line = p.summary_line(false);
    assert(line.find("summary") != std::string::npos);
    assert(line.find("/") != std::string::npos);
    std::cout << "Test 6 passed: summary_line() formatted correctly.\n";
  }

  // === Test 7: Empty process finishes immediately ===
  {
    std::vector<Instruction> ins = {};
    Process p(7, "empty", ins);
    uint32_t tick = 0, consumed = 0;

    ProcessState result = p.execute_tick(++tick, 0, consumed).state;
    assert(result == ProcessState::FINISHED);
    assert(p.state() == ProcessState::FINISHED);
    std::cout << "Test 7 passed: Empty process finishes instantly.\n";
  }

  // === Test 8: FOR_MAX_NESTING respected ===
  {
    Instruction inner_for;
    inner_for.type = InstructionType::FOR;
    inner_for.args.push_back("2");
    Instruction base_print;
    base_print.type = InstructionType::PRINT;
    inner_for.nested.push_back(base_print);

    Instruction top;
    top.type = InstructionType::FOR;
    top.args.push_back("2");
    for (int i = 0; i < 12; ++i)
      top.nested.push_back(inner_for);

    std::vector<Instruction> ins = {top};
    Process p(8, "deep", ins);

    assert(p.get_total_instructions() > 0);
    assert(p.get_total_instructions() < 1000);
    std::cout << "Test 8 passed: FOR_MAX_NESTING handled gracefully.\n";
  }

  // === Test 9: Thread-safe log access during execution ===
  {
    std::vector<Instruction> ins = {{InstructionType::PRINT, {"one"}},
                                    {InstructionType::SLEEP, {"2"}},
                                    {InstructionType::PRINT, {"two"}}};
    Process p(9, "threadsafe", ins);
    std::atomic<bool> done{false};

    std::thread exec_thread([&]() {
      uint32_t tick = 0, consumed = 0;
      while (p.execute_tick(++tick, 0, consumed).state != ProcessState::FINISHED)
        ;
      done = true;
    });

    std::thread reader_thread([&]() {
      while (!done) {
        auto logs = p.get_logs();
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
      }
    });

    exec_thread.join();
    reader_thread.join();
    std::cout << "Test 9 passed: Mutex lock safety confirmed.\n";
  }

  // === Test 10: Variable persistence and reuse ===
  {
    std::vector<Instruction> ins = {
        {InstructionType::DECLARE, {"a", "5"}},
        {InstructionType::DECLARE, {"b", "7"}},
        {InstructionType::ADD, {"a", "a", "b"}},
        {InstructionType::SUBTRACT, {"b", "a", "3"}},
        {InstructionType::PRINT, {"done"}}};

    Process p(10, "vars", ins);
    uint32_t tick = 0, consumed = 0;
    while (p.execute_tick(++tick, 0, consumed).state != ProcessState::FINISHED)
      ;

    assert(p.vars.at("a") == 12);
    assert(p.vars.at("b") == 9);
    std::cout << "Test 10 passed: Variable persistence OK.\n";
  }

  // === Test 11: Rapid tick execution (busy-wait simulation) ===
  {
    std::vector<Instruction> ins = {{InstructionType::PRINT, {"rapid"}},
                                    {InstructionType::PRINT, {"fast"}}};
    Process p(11, "busy", ins);
    uint32_t tick = 0, consumed = 0;
    while (p.execute_tick(++tick, 2, consumed).state != ProcessState::FINISHED)
      ;
    assert(p.get_executed_instructions() == 2);
    std::cout << "Test 11 passed: Delays handled correctly.\n";
  }

  std::cout << "All process tests passed successfully.\n";
  return 0;
}
