#include "../include/process_generator.hpp"
#include "../include/instruction.hpp"
#include "../include/process.hpp"
#include <chrono>
#include <memory>
#include <random>
#include <thread>

// TODO:
// - Make next_id_ atomic or protected explicitly.
// - Add DEBUG_GENERATOR logging to record created process id/name and
// instruction counts (useful for debugging expansion).
// - Modify generator to control post-unroll instruction budget rather than
// top-level count.
// - Add a small unit test / smoke harness to generate a process, unroll it, and
// assert the unrolled size is within a limit.

// Utility: random integer in [min, max]
static uint32_t rand_range(uint32_t min, uint32_t max) {
  // Guard: if min > max, swap to avoid UB
  if (min > max) {
    uint32_t t = min;
    min = max;
    max = t;
  }
  static thread_local std::mt19937 rng(std::random_device{}());
  std::uniform_int_distribution<uint32_t> dist(min, max);
  return dist(rng);
}

// Utility: generate a random instruction
// Supports nested FOR instructions up to a limited depth to avoid runaway
// expansion when the scheduler unrolls FORs.
static Instruction random_instruction(int depth = 0) {
  InstructionType types[] = {InstructionType::PRINT, InstructionType::DECLARE,
                             InstructionType::ADD,   InstructionType::SUBTRACT,
                             InstructionType::SLEEP, InstructionType::FOR};
  // pick a random index using the actual array size (safer than hardcoding)
  const uint32_t types_count =
      static_cast<uint32_t>(sizeof(types) / sizeof(types[0]));

  // Prevent deep FOR nesting. If at or beyond max depth, disallow FOR.
  InstructionType t;
  do {
    t = types[rand_range(0, types_count - 1)];
  } while (t == InstructionType::FOR && depth >= FOR_MAX_NESTING);

  Instruction instr;
  instr.type = t;

  switch (t) {
  case InstructionType::PRINT:
    instr.args.push_back("Hello");
    break;
  case InstructionType::DECLARE:
    instr.args.push_back("x");
    instr.args.push_back(std::to_string(rand_range(0, 50)));
    break;
  case InstructionType::ADD:
  case InstructionType::SUBTRACT:
    instr.args.push_back("x");
    instr.args.push_back(std::to_string(rand_range(0, 20)));
    instr.args.push_back(std::to_string(rand_range(0, 20)));
    break;
  case InstructionType::SLEEP:
    instr.args.push_back(std::to_string(rand_range(1, 3)));
    break;
  case InstructionType::FOR: {
    // FOR(repeats) with nested instructions.
    // repeats between 1 and 3 to avoid massive expansion when unrolled.
    uint32_t repeats = rand_range(1, 3);
    instr.args.push_back(std::to_string(repeats));

    // Generate a small number of nested instructions (1-3)
    uint32_t nested_count = rand_range(1, 3);
    for (uint32_t i = 0; i < nested_count; ++i) {
      // Recursively generate nested instructions, increasing depth
      Instruction nested = random_instruction(depth + 1);
      instr.nested.push_back(std::move(nested));
    }
    break;
  }
  default:
    break;
  }
  return instr;
}

// Estimate unrolled size for a single instruction (handles nested FORs)
static uint32_t estimate_unrolled_size_for_instr(const Instruction &instr) {
  if (instr.type != InstructionType::FOR) {
    return 1;
  }
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
    nested_total += estimate_unrolled_size_for_instr(n);
  return repeats * nested_total;
}

// Estimate unrolled size for a vector of instructions
static uint32_t estimate_unrolled_size(const std::vector<Instruction> &ins) {
  uint32_t total = 0;
  for (const auto &i : ins)
    total += estimate_unrolled_size_for_instr(i);
  return total;
}

// Constructor
ProcessGenerator::ProcessGenerator(const Config &cfg, Scheduler &sched)
    : cfg_(cfg), sched_(sched) {}

// Public helper used by loop and tests: generate up to target_top_level
// top-level instructions while respecting the configured budget. Returns
// the generated instructions and writes the estimated unrolled size to
// estimated_size.
std::vector<Instruction>
ProcessGenerator::generate_instructions(uint32_t target_top_level,
                                        uint32_t &estimated_size) {
  estimated_size = 0;
  std::vector<Instruction> ins;
  ins.reserve(target_top_level);
  for (uint32_t i = 0; i < target_top_level; ++i) {
    Instruction instr = random_instruction(0);
    uint32_t instr_size = estimate_unrolled_size_for_instr(instr);
    if (cfg_.max_unrolled_instructions > 0 &&
        estimated_size + instr_size > cfg_.max_unrolled_instructions) {
      break;
    }
    estimated_size += instr_size;
    ins.push_back(std::move(instr));
  }
  return ins;
}

// Start thread loop
void ProcessGenerator::start() {
  if (running_.load())
    return;
  running_.store(true);
  thread_ = std::thread(&ProcessGenerator::loop, this);
}

// Stop generator
void ProcessGenerator::stop() {
  running_.store(false);
  if (thread_.joinable())
    thread_.join();
}

// Internal loop
void ProcessGenerator::loop() {
  using namespace std::chrono_literals;
  while (running_.load()) {
    // sleep between process generations
    std::this_thread::sleep_for(std::chrono::seconds(cfg_.batch_process_freq));

    // Generate process instructions while respecting configured budget
    uint32_t num_instructions = rand_range(cfg_.min_ins, cfg_.max_ins);
    uint32_t estimated_size = 0;
    std::vector<Instruction> ins;
    ins.reserve(num_instructions);
    for (uint32_t i = 0; i < num_instructions; ++i) {
      Instruction instr = random_instruction(0);
      uint32_t instr_size = estimate_unrolled_size_for_instr(instr);
      if (cfg_.max_unrolled_instructions > 0 &&
          estimated_size + instr_size > cfg_.max_unrolled_instructions) {
        // exceeding budget: stop adding more top-level instructions
        break;
      }
      estimated_size += instr_size;
      ins.push_back(std::move(instr));
    }

    // Assign id first (post-increment) and use the same id for the name to
    // avoid off-by-one mismatch between id and name.
    uint32_t id = next_id_.fetch_add(1);
    auto process = std::make_shared<Process>(id, "P" + std::to_string(id), ins);

    sched_.submit_process(process);
  }
}
