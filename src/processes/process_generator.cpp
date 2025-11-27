#include "processes/process_generator.hpp"
#include "processes/instruction.hpp"
#include "processes/process.hpp"
#include <chrono>
#include <iomanip>
#include <iostream>
#include <memory>
#include <random>
#include <sstream>
#include <thread>

// Enable debug logging for the generator by uncommenting:
// #define DEBUG_GENERATOR

/**
 * Convert InstructionType enum to human-readable string representation
 *
 * This helper function provides consistent string conversion for all
 * instruction types, primarily used for debug output and logging.
 *
 * @param t The InstructionType to convert
 * @return String representation of the instruction type
 */
static std::string inst_type_to_string(InstructionType t) {
  switch (t) {
  case InstructionType::PRINT:
    return "PRINT";
  case InstructionType::DECLARE:
    return "DECLARE";
  case InstructionType::ADD:
    return "ADD";
  case InstructionType::SUBTRACT:
    return "SUBTRACT";
  case InstructionType::SLEEP:
    return "SLEEP";
  case InstructionType::FOR:
    return "FOR";
  case InstructionType::READ:
    return "READ";
  case InstructionType::WRITE:
    return "WRITE";
  default:
    return "UNKNOWN";
  }
}

/**
 * Create a human-readable string representation of an Instruction
 *
 * Formats an Instruction object into a debug-friendly string showing the type
 * and arguments. For FOR instructions, shows the number of nested instructions
 * but does not expand them to keep output concise.
 *
 * Format:
 * - Basic instructions: TYPE(arg1, arg2, ...)
 * - FOR instructions: FOR(repeats){N nested}
 *
 * @param instr The Instruction to convert to string
 * @return Human-readable representation of the instruction
 */
static std::string instr_to_string(const Instruction &instr) {
  std::ostringstream oss;
  oss << inst_type_to_string(instr.type);
  if (!instr.args.empty()) {
    oss << "(";
    for (size_t i = 0; i < instr.args.size(); ++i) {
      if (i)
        oss << ", ";
      oss << instr.args[i];
    }
    oss << ")";
  }
  if (instr.type == InstructionType::FOR && !instr.nested.empty()) {
    oss << "{" << instr.nested.size() << " nested}";
  }
  return oss.str();
}

/**
 * Utility: random integer in [min, max]
 *
 * Notes:
 * - Uses a thread-local PRNG (`std::mt19937`) seeded from `std::random_device`.
 * - If `min > max` the values are swapped to avoid undefined behaviour.
 * - This function is safe to call from multiple threads because the RNG is
 *   thread-local.
 *
 * @param min Lower inclusive bound
 * @param max Upper inclusive bound
 * @return A uniformly distributed random integer in [min, max]
 */
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

/**
 * Generate a random Instruction instance.
 *
 * The generator can produce any InstructionType including `FOR`. When a
 * `FOR` instruction is generated it will include a repeat count in
 * `args[0]` and a small `nested` vector of child instructions. To prevent
 * runaway nesting the generator accepts a `depth` parameter and will avoid
 * creating `FOR` when `depth >= FOR_MAX_NESTING`.
 *
 * Behaviour:
 * - PRINT: args[0] = "Hello"
 * - DECLARE: args = { "x", <value> }
 * - ADD/SUBTRACT: args = { "x", <a>, <b> }
 * - SLEEP: args = { <ticks> }
 * - FOR: args = { <repeats> }, nested = small list of nested instructions
 *
 * @param depth Current recursion depth for nested FOR generation (0 =
 * top-level)
 * @return A randomly constructed Instruction
 */
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

/**
 * Estimate unrolled size for a single instruction (handles nested FORs)
 *
 * This recursively inspects a top-level instruction and computes how many
 * primitive instructions it will expand to once FOR loops are unrolled.
 * For a non-FOR instruction this is 1. For a FOR instruction it is:
 *   repeats * sum(estimate(nested_i))
 *
 * Edge cases:
 * - Malformed repeat counts (non-numeric) are treated as 1.
 * - Empty nested bodies result in 0 contribution.
 *
 * @param instr Instruction to estimate
 * @return Estimated number of primitive instructions after unrolling
 */
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

/**
 * Estimate unrolled size for a vector of instructions
 *
 * @param ins Vector of top-level instructions
 * @return Total estimated number of primitive instructions after unrolling
 */
static uint32_t estimate_unrolled_size(const std::vector<Instruction> &ins) {
  uint32_t total = 0;
  for (const auto &i : ins)
    total += estimate_unrolled_size_for_instr(i);
  return total;
}

/**
 * Construct a new ProcessGenerator instance
 *
 * The ProcessGenerator creates processes with random instructions according to
 * the provided configuration. It submits these processes to the scheduler at
 * intervals defined by Config::batch_process_freq.
 *
 * Thread Safety:
 * - Safe to construct from any thread
 * - References to cfg and sched must remain valid for generator lifetime
 *
 * @param cfg Configuration controlling process generation (instruction counts,
 *            budgets, timing)
 * @param sched Scheduler that will receive generated processes
 */
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
#ifdef DEBUG_GENERATOR
    {
      std::ostringstream dbg;
      dbg << "generator: candidate " << instr_to_string(instr)
          << " -> estimated_size=" << instr_size;
      std::clog << dbg.str() << std::endl;
    }
#endif
    if (cfg_.max_unrolled_instructions > 0 &&
        estimated_size + instr_size > cfg_.max_unrolled_instructions) {
#ifdef DEBUG_GENERATOR
      std::ostringstream dbg;
      dbg << "generator: budget exceeded (estimated " << estimated_size
          << ", instr would add " << instr_size << ") - stopping";
      std::clog << dbg.str() << std::endl;
#endif
      break;
    }
    estimated_size += instr_size;
    ins.push_back(std::move(instr));
  }
  return ins;
}

// /**
//  * Revised generate_instructions for test case 4.
//  */
// std::vector<Instruction>
// ProcessGenerator::generate_instructions(uint32_t count, uint32_t &est_size) {
//   std::vector<Instruction> result;
//   result.reserve(count);

//   // Ensure variable x exists with value 0 (DECLARATION)
//   Instruction decl;
//   decl.type = InstructionType::DECLARE;
//   decl.args = {"x", "0"};
//   result.push_back(decl);

//   bool print_next = true;
//   for (uint32_t i = 0; i < count; ++i) {
//     Instruction instr;
//     if (print_next) {
//       instr.type = InstructionType::PRINT;
//       instr.args = {"Value from: x"};
//     } else {
//       instr.type = InstructionType::ADD;
//       instr.args = {"x", "x", std::to_string(rand() % 10 + 1)};
//     }
//     result.push_back(instr);
//     print_next = !print_next;
//   }

//   est_size = static_cast<uint32_t>(result.size());
//   return result;
// }

/**
 * Start the process generation thread
 *
 * Creates and starts a background thread that periodically generates new
 * processes according to the configuration. If already running, this is a
 * no-op.
 *
 * Thread Safety:
 * - Safe to call from any thread
 * - Uses atomic flag to prevent multiple starts
 * - Must be paired with stop() to clean up resources
 */
void ProcessGenerator::start() {
  if (running_.load())
    return;
  running_.store(true);
  // Seed one process immediately so users see activity right after `scheduler-start`
  try {
    uint32_t est_size_immediate = 0;
    uint32_t initial_top = rand_range(cfg_.min_ins, cfg_.max_ins);
    auto ins0 = generate_instructions(initial_top, est_size_immediate);
    uint32_t id0 = next_id_.fetch_add(1);
    std::ostringstream name0; name0 << "p" << std::setw(2) << std::setfill('0') << id0;
    auto p0 = std::make_shared<Process>(id0, name0.str(), ins0);
    sched_.submit_process(p0);
  } catch (...) { /* best effort seed; ignore errors */ }
#ifdef DEBUG_GENERATOR
  std::clog << "generator: starting" << std::endl;
#endif
  thread_ = std::thread(&ProcessGenerator::loop, this);
}

/**
 * Stop the process generation thread
 *
 * Signals the generator thread to stop and waits for it to complete. Safe to
 * call multiple times. After stopping, the generator can be restarted with
 * start().
 *
 * Thread Safety:
 * - Safe to call from any thread
 * - Uses atomic flag for thread-safe shutdown
 * - Blocks until generator thread joins
 */
void ProcessGenerator::stop() {
  running_.store(false);
#ifdef DEBUG_GENERATOR
  std::clog << "generator: stopping" << std::endl;
#endif
  if (thread_.joinable())
    thread_.join();
}

/**
 * Main generator thread loop
 *
 * Revised:
 * - batch_process_freq is measured in CPU cycles (scheduler ticks), not
 * milliseconds.
 * - This loop now synchronizes process generation with the scheduler’s tick
 * counter.
 *
 * Behavior:
 *   Every `batch_process_freq` scheduler ticks, this thread generates one new
 * process. Each process is constructed with a random number of instructions
 * (within [min_ins, max_ins]), constrained by the configured
 * max_unrolled_instructions budget.
 *
 * Thread Safety:
 * - Runs in a dedicated background thread.
 * - Polls the scheduler’s current tick safely (no blocking interaction).
 * - Uses atomic flag `running_` for coordinated shutdown.
 */
void ProcessGenerator::loop() {
  uint32_t last_generated_tick = 0;

  while (running_.load()) {
    uint32_t current_tick = sched_.current_tick();
    if (sched_.get_total_active_processes() <=cfg_.max_generated_processes && current_tick - last_generated_tick >= cfg_.batch_process_freq) {
      // generate one process
      uint32_t num_instructions = rand_range(cfg_.min_ins, cfg_.max_ins);
      uint32_t estimated_size = 0;
      auto ins = generate_instructions(num_instructions, estimated_size);

      uint32_t id = next_id_.fetch_add(1);
      std::ostringstream name;
      name << "p" << std::setw(2) << std::setfill('0') << id;
      auto process = std::make_shared<Process>(id, name.str(), ins);

      sched_.submit_process(process);
      last_generated_tick = current_tick;
    }

    // sleep just a bit to avoid busy-waiting
    std::this_thread::sleep_for(
        std::chrono::milliseconds(cfg_.scheduler_tick_delay * 4 + 10));
  }
}
