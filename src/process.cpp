#include "../include/process.hpp"
#include "../include/instruction.hpp"
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <stdexcept>

// Enable debug logging by uncommenting:
// #define DEBUG_PROCESS

/**
 * Convert instruction type to string for debug output
 */
static std::string inst_type_to_string(InstructionType type) {
  switch (type) {
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
  default:
    return "UNKNOWN";
  }
}

/**
 * Convert process state enum to readable string
 */
std::string Process::get_state_string() {
  switch (m_state) {
  case ProcessState::NEW:
    return "NEW";
  case ProcessState::READY:
    return "READY";
  case ProcessState::RUNNING:
    return "RUNNING";
  case ProcessState::WAITING:
    return "WAITING";
  case ProcessState::BLOCKED_PAGE_FAULT:
    return "BLOCKED_PAGE_FAULT";
  case ProcessState::SWAPPED_OUT:
    return "SWAPPED_OUT";
  case ProcessState::FINISHED:
    return "FINISHED";
  default:
    return "UNKNOWN";
  }
}

/**
 * Helper: clamp value to uint16_t range [0, 65535]
 * Used for all arithmetic operations to prevent overflow
 */
static uint16_t clamp_uint16(int64_t v) {
  if (v < 0)
    return 0;
  if (v > 65535)
    return 65535;
  return static_cast<uint16_t>(v);
}

/**
 * Helper: Check if a string represents a valid number
 * Accepts optional +/- prefix followed by digits
 * @param s String to check
 * @return true if string is a valid number format
 */
static bool is_number(const std::string &s) {
  if (s.empty())
    return false;
  size_t i = (s[0] == '+' || s[0] == '-') ? 1 : 0;
  for (; i < s.size(); ++i)
    if (!isdigit((unsigned char)s[i]))
      return false;
  return true;
}

/**
 * Expands FOR loops by unrolling them into a flat sequence of instructions
 *
 * Strategy:
 * 1. If not a FOR, append instruction as-is
 * 2. If FOR loop:
 *    - Parse repeat count from args[0]
 *    - For each repetition:
 *      - For each nested instruction:
 *        - If nested FOR, recurse with depth+1
 *        - Otherwise append instruction
 * 3. Depth limit defined by FOR_MAX_NESTING (see `include/instruction.hpp`)
 *
 * Example:
 *   FOR(2)
 *     PRINT(Hi)
 *     FOR(3)
 *       ADD(x,x,1)
 * Unrolls to:
 *   PRINT(Hi)
 *   ADD(x,x,1)
 *   ADD(x,x,1)
 *   ADD(x,x,1)
 *   PRINT(Hi)
 *   ADD(x,x,1)
 *   ADD(x,x,1)
 *   ADD(x,x,1)
 *
 * @param instr Instruction to potentially unroll
 * @param out Vector to append unrolled instructions to
 * @param depth Current recursion depth (max 10)
 */
static void unroll_instruction(const Instruction &instr,
                               std::vector<Instruction> &out, int depth = 0) {
  if (instr.type != InstructionType::FOR) {
    out.push_back(instr);
    return;
  }

  if (depth >= FOR_MAX_NESTING) {
    for (const auto &inner : instr.nested)
      out.push_back(inner);
    return;
  }

  if (instr.args.empty()) {
    for (const auto &inner : instr.nested)
      out.push_back(inner);
    return;
  }

  int repeats = 0;
  try {
    repeats = std::stoi(instr.args[0]);
  } catch (...) {
    repeats = 0;
  }

  if (repeats <= 0) {
    for (const auto &inner : instr.nested)
      out.push_back(inner);
    return;
  }

  for (int r = 0; r < repeats; ++r) {
    for (const auto &inner : instr.nested) {
      if (inner.type == InstructionType::FOR)
        unroll_instruction(inner, out, depth + 1);
      else
        out.push_back(inner);
    }
  }
}

/**
 * Constructor: preprocesses and unrolls FOR loops
 */
Process::Process(uint32_t id, const std::string &name,
                 std::vector<Instruction> ins)
    : m_id(id), m_name(name), m_state(ProcessState::NEW), m_instr() {

  // Pre-expand FOR instructions
  for (const auto &inst : ins) {
    if (inst.type == InstructionType::FOR)
      unroll_instruction(inst, m_instr, 0);
    else
      m_instr.push_back(inst);
  }

  // Initialize metrics
  m_metrics.total_instructions = static_cast<uint32_t>(m_instr.size());
  m_metrics.executed_instructions = 0;
  m_metrics.start_time = std::time(nullptr);
  m_metrics.finish_time = 0;
  m_metrics.core_id = UINT32_MAX;

#ifdef DEBUG_PROCESS
  std::ostringstream dbg;
  dbg << "Created process " << m_name << " (id=" << m_id << ") with "
      << m_instr.size() << " instructions";
  m_logs.push_back(dbg.str());
#endif
}

// === Basic Accessors ===
uint32_t Process::id() const { return m_id; }
std::string Process::name() const { return m_name; }

ProcessState Process::state() {
  std::lock_guard<std::mutex> lk(m_mutex);
  return m_state;
}

void Process::set_state(ProcessState s) {
  std::lock_guard<std::mutex> lk(m_mutex);
  m_state = s;
}

void Process::set_core_id(uint32_t core) {
  std::lock_guard<std::mutex> lk(m_mutex);
  m_metrics.core_id = core;
}

uint32_t Process::get_core_id() const { return m_metrics.core_id; }
uint32_t Process::get_total_instructions() const {
  return m_metrics.total_instructions;
}
uint32_t Process::get_executed_instructions() const {
  return m_metrics.executed_instructions;
}

std::vector<std::string> Process::get_logs() {
  std::lock_guard<std::mutex> lk(m_mutex);
  return m_logs;
}

/**
 * Formats a concise single-line summary for screen -ls / report-util
 */
std::string Process::summary_line(bool colorize) const {
  auto fmt_time = [](std::time_t t) -> std::string {
    if (t == 0)
      return "(--)";
    std::tm tm_buf{};
#ifdef _WIN32
    localtime_s(&tm_buf, &t);
#else
    localtime_r(&t, &tm_buf);
#endif
    std::ostringstream ts;
    ts << "(" << std::put_time(&tm_buf, "%m/%d/%Y %I:%M:%S%p") << ")";
    return ts.str();
  };

  std::ostringstream oss;
  std::lock_guard<std::mutex> lk(m_mutex);

  oss << std::left << std::setw(12) << m_name << " "
      << fmt_time(m_metrics.start_time) << "   ";

  if (m_state == ProcessState::FINISHED) {
    oss << "Finished   ";
  } else {
    if (m_metrics.core_id != UINT32_MAX)
      oss << "Core: " << m_metrics.core_id << "   ";
    else
      oss << "Core: -   ";
  }

  oss << std::right << m_metrics.executed_instructions << " / "
      << m_metrics.total_instructions;

  return oss.str();
}

/**
 * SMI summary with logs (unchanged)
 */
std::string Process::smi_summary() {
  std::lock_guard<std::mutex> lk(m_mutex);
  std::ostringstream oss;
  oss << "Process " << m_name << " [" << get_state_string() << "]\n";
  oss << "PC: " << pc << " / " << m_instr.size() << "\n";
  oss << "Logs:\n";
  for (const auto &line : m_logs)
    oss << line << "\n";
  if (m_state == ProcessState::FINISHED) {
    oss << "Finished!\n";
  }
  return oss.str();
}

/**
 * Parses a token into a uint16_t value
 * Token can be either:
 * 1. A literal number (will be clamped to uint16_t range)
 * 2. A variable name (returns current value, or 0 if undefined)
 *
 * @param token String to parse
 * @param vars Current variable storage map
 * @return Parsed and clamped value
 */
static uint16_t
read_token_value(const std::string &token,
                 std::unordered_map<std::string, uint16_t> &vars) {
  if (is_number(token)) {
    try {
      return clamp_uint16(std::stoll(token));
    } catch (...) {
      return 0;
    }
  }
  auto it = vars.find(token);
  if (it == vars.end()) {
    vars[token] = 0;
    return 0;
  }
  return it->second;
}

/**
 * Sets a variable's value in the storage map
 * Creates the variable if it doesn't exist
 *
 * @param name Variable name
 * @param v Value to set (already clamped to uint16_t)
 * @param vars Variable storage map to update
 */
static void set_var_value(const std::string &name, uint16_t v,
                          std::unordered_map<std::string, uint16_t> &vars) {
  vars[name] = v;
}

/**
 * Main execution tick - Executes one instruction or handles sleep/delay
 *
 * Process execution states:
 * 1. Sleep state (m_sleep_remaining > 0):
 *    - Decrement counter and stay in WAITING state
 *    - Transition to READY when sleep complete
 * 2. Normal execution:
 *    - Execute one instruction
 *    - Handle delays if delays_per_exec > 0
 * 3. Completion:
 *    - Set FINISHED state when pc reaches end
 *    - Record finished_tick in metrics
 *
 * @param global_tick Current scheduler tick count
 * @param delays_per_exec Number of busy-wait ticks after each instruction
 * @param consumed_ticks Output parameter for ticks used this execution
 * @return true if process finished, false if more work remains
 */
bool Process::execute_tick(uint32_t global_tick, uint32_t delays_per_exec,
                           uint32_t &consumed_ticks) {
  consumed_ticks = 1; // default one tick consumed
  std::lock_guard<std::mutex> lk(m_mutex);

  // Handle busy-wait delay (simulates CPU hold cycles)
  if (m_delay_remaining > 0) {
    --m_delay_remaining;
    m_state = ProcessState::RUNNING; // stays running but not executing new inst
    consumed_ticks = 1;
    return false;
  }

  if (m_state == ProcessState::FINISHED) {
#ifdef DEBUG_PROCESS
    std::ostringstream dbg;
    dbg << m_name << ": Already FINISHED at tick " << global_tick
        << " (finished at " << m_metrics.finished_tick << ")";
    m_logs.push_back(dbg.str());
#endif
    return true;
  }

  // If currently sleeping due to SLEEP instruction
  if (m_sleep_remaining > 0) {
    // sleeping is WAITING and consumes 1 tick per call
    --m_sleep_remaining;
    ProcessState new_state =
        (m_sleep_remaining == 0) ? ProcessState::READY : ProcessState::WAITING;
#ifdef DEBUG_PROCESS
    if (new_state != m_state) {
      std::ostringstream dbg;
      dbg << m_name << ": State "
          << (m_state == ProcessState::WAITING ? "WAITING" : "RUNNING")
          << " -> " << (new_state == ProcessState::READY ? "READY" : "WAITING")
          << " (sleep=" << m_sleep_remaining << ")";
      m_logs.push_back(dbg.str());
    }
#endif
    m_state = new_state;
    return false;
  }

  // If no instructions left
  if (pc >= m_instr.size()) {
    m_state = ProcessState::FINISHED;
    m_metrics.finished_tick = global_tick;
    m_metrics.finish_time = std::time(nullptr);
    return true;
  }

  // Mark running
  m_state = ProcessState::RUNNING;

  const Instruction &inst = m_instr[pc];

  switch (inst.type) {
  case InstructionType::PRINT: {
    // PRINT(msg) or PRINT("Hello world from <process_name>!")
    std::string out;
    if (!inst.args.empty()) {
      out = inst.args[0];
    } else {
      std::ostringstream tmp;
      tmp << "Hello world from " << m_name << "!";
      out = tmp.str();
    }
    m_logs.push_back(out);
    ++pc;
    break;
  }

  case InstructionType::DECLARE: {
    // args: var, value
    if (inst.args.size() >= 2) {
      const std::string &var = inst.args[0];
      const std::string &valtok = inst.args[1];
      uint16_t v = read_token_value(valtok, vars);
      set_var_value(var, v, vars);
#ifdef DEBUG_PROCESS
      std::ostringstream dbg;
      dbg << m_name << ": DECLARE " << var << " = " << v;
      m_logs.push_back(dbg.str());
#endif
    } else if (inst.args.size() == 1) {
      set_var_value(inst.args[0], 0, vars);
#ifdef DEBUG_PROCESS
      std::ostringstream dbg;
      dbg << m_name << ": DECLARE " << inst.args[0] << " = 0";
      m_logs.push_back(dbg.str());
#endif
    }
    ++pc;
    break;
  }

  case InstructionType::ADD: {
    // ADD(var1, var2/value, var3/value) -> var1 = var2 + var3
    if (inst.args.size() >= 3) {
      const std::string &dst = inst.args[0];
      uint16_t a = read_token_value(inst.args[1], vars);
      uint16_t b = read_token_value(inst.args[2], vars);
      uint32_t sum = static_cast<uint32_t>(a) + static_cast<uint32_t>(b);
      uint16_t result = clamp_uint16(sum);
      set_var_value(dst, result, vars);
#ifdef DEBUG_PROCESS
      std::ostringstream dbg;
      dbg << m_name << ": ADD " << dst << " = " << a << " + " << b << " = "
          << result << (sum > 65535 ? " (clamped)" : "");
      m_logs.push_back(dbg.str());
#endif
    }
    ++pc;
    break;
  }

  case InstructionType::SUBTRACT: {
    if (inst.args.size() >= 3) {
      const std::string &dst = inst.args[0];
      int32_t a = static_cast<int32_t>(read_token_value(inst.args[1], vars));
      int32_t b = static_cast<int32_t>(read_token_value(inst.args[2], vars));
      int64_t res = static_cast<int64_t>(a) - static_cast<int64_t>(b);
      uint16_t result = clamp_uint16(res);
      set_var_value(dst, result, vars);
#ifdef DEBUG_PROCESS
      std::ostringstream dbg;
      dbg << m_name << ": SUBTRACT " << dst << " = " << a << " - " << b << " = "
          << result << (res < 0 || res > 65535 ? " (clamped)" : "");
      m_logs.push_back(dbg.str());
#endif
    }
    ++pc;
    break;
  }

  case InstructionType::SLEEP: {
    // SLEEP(X) -> relinquish CPU for X ticks (WAITING)
    uint32_t ticks = 0;
    if (!inst.args.empty() && is_number(inst.args[0])) {
      try {
        ticks = static_cast<uint32_t>(std::stoul(inst.args[0]));
      } catch (...) {
        ticks = 0;
      }
    }
    if (ticks == 0) {
      ++pc; // zero sleep: just continue
    } else {
      m_sleep_remaining = ticks;
      m_state = ProcessState::WAITING;
      ++pc; // advance PC so when sleep ends we resume after SLEEP
    }
    break;
  }

  case InstructionType::FOR: {
    // FOR should have been unrolled in constructor. If encountered, skip
    // safely.
    ++pc;
    break;
  }

  default:
    // Unknown instruction: skip
    ++pc;
    break;
  }

  // Increment executed-instruction count (skip FOR)
  if (inst.type != InstructionType::FOR)
    ++m_metrics.executed_instructions;

#ifdef DEBUG_PROCESS
  std::ostringstream dbg;
  dbg << m_name << "[pc=" << pc << "]: " << inst_type_to_string(inst.type);
  if (!inst.args.empty()) {
    dbg << "(";
    for (size_t i = 0; i < inst.args.size(); ++i) {
      if (i > 0)
        dbg << ", ";
      dbg << inst.args[i];
    }
    dbg << ")";
  }
  m_logs.push_back(dbg.str());
#endif

  // Set busy-wait delay if configured
  if (delays_per_exec > 0 && pc <= m_instr.size()) {
    // Only set delay if more instructions remain
    m_delay_remaining = delays_per_exec;
  }

  // If pc reached end after increment
  if (pc >= m_instr.size()) {
    m_state = ProcessState::FINISHED;
    m_metrics.finished_tick = global_tick;
    m_metrics.finish_time = std::time(nullptr);
    return true;
  }

  return false;
}
