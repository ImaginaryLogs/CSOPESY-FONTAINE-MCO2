#include "processes/process.hpp"
#include "processes/instruction.hpp"
#include "paging/memory_manager.hpp"
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <ctime>
#include <iomanip>
#include <iostream>
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

ProcessState Process::get_state(){
  return this->m_state;
};

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

void Process::set_name(std::string name){
  this->m_name = name;
}

uint32_t Process::get_core_id() const { return m_metrics.core_id; }
uint32_t Process::get_total_instructions() const {
  return m_metrics.total_instructions;
}
uint32_t Process::get_executed_instructions() const {
  return m_metrics.executed_instructions;
}
uint32_t Process::get_remaining_sleep_ticks() const {
  return m_sleep_remaining;
}

std::vector<std::string> Process::get_logs() {
  std::lock_guard<std::mutex> lk(m_mutex);
  return m_logs;
}

// === State Query Helpers ===
bool Process::is_new() const noexcept {
  return m_state == ProcessState::NEW;
}
bool Process::is_ready() const noexcept {
  return m_state == ProcessState::READY;
}
bool Process::is_running() const noexcept {
  return m_state == ProcessState::RUNNING;
}
bool Process::is_waiting() const noexcept {
  return m_state == ProcessState::WAITING;
}
bool Process::is_finished() const noexcept {
  return m_state == ProcessState::FINISHED;
}
bool Process::is_swapped() const noexcept {
  return m_state == ProcessState::SWAPPED_OUT;
}
bool Process::is_blocked() const noexcept {
  return m_state == ProcessState::BLOCKED_PAGE_FAULT;
}

bool is_yielded(ProcessReturnContext context) noexcept {
  return context.state == ProcessState::READY ||
         context.state == ProcessState::WAITING ||
         context.state == ProcessState::FINISHED ||
         context.state == ProcessState::BLOCKED_PAGE_FAULT;
}

// === State Transition Helpers ===
void Process::mark_ready() { set_state(ProcessState::READY); }
void Process::mark_running() { set_state(ProcessState::RUNNING); }
void Process::mark_waiting() { set_state(ProcessState::WAITING); }
void Process::mark_swapped() { set_state(ProcessState::SWAPPED_OUT); }
void Process::mark_finished(uint32_t tick) {
  std::lock_guard<std::mutex> lk(m_mutex);
  m_state = ProcessState::FINISHED;
  m_metrics.finished_tick = tick;
  m_metrics.finish_time = std::time(nullptr);
}

// === Execution Info Helpers ===
bool Process::has_instructions_remaining() const noexcept {
  return pc < m_instr.size();
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
read_token_value_old(const std::string &token,
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
 * @param global_tick Current scheduler tick count
 * @param delays_per_exec Number of busy-wait ticks after each instruction
 * @param consumed_ticks Output parameter for ticks used this execution
 * @return ProcessState corresponding
 */
ProcessReturnContext Process::execute_tick(uint32_t global_tick,
                                   uint32_t delays_per_exec,
                                   uint32_t &consumed_ticks) {
  consumed_ticks = 1; // default one tick consumed
  std::lock_guard<std::mutex> lk(m_mutex);

  // --- Case 1: Delay / busy wait ---
  if (m_delay_remaining > 0) {
    --m_delay_remaining;
    m_state = ProcessState::RUNNING;
    return {ProcessState::RUNNING, {}};
  }

  // --- Case 2: Finished already ---
  if (m_state == ProcessState::FINISHED) {
#ifdef DEBUG_PROCESS
    std::ostringstream dbg;
    dbg << m_name << ": Already FINISHED at tick " << global_tick
        << " (finished at " << m_metrics.finished_tick << ")";
    m_logs.push_back(dbg.str());
#endif
    return {ProcessState::FINISHED, {}};
  }

  // --- Case 3: Currently sleeping ---
  if (m_sleep_remaining > 0) {
    --m_sleep_remaining;
#ifdef DEBUG_PROCESS
    {
      std::ostringstream dbg;
      dbg << m_name << ": Sleeping, remaining=" << m_sleep_remaining;
      m_logs.push_back(dbg.str());
    }
#endif
    if (m_sleep_remaining == 0) {
      m_state = ProcessState::READY;
      return {ProcessState::READY, {}}; // wakes up, can be rescheduled
    } else {
      m_state = ProcessState::WAITING;
      return {ProcessState::WAITING, {std::to_string(m_sleep_remaining)}}; // still sleeping, yield CPU
    }
  }

  // --- Case 3.5: Blocked on Page Fault (Resumed) ---
  // If we were blocked, and now we are scheduled, it means the page is ready (Scheduler moved us to READY).
  // So we retry the instruction at `pc`.
  // No special handling needed here, just proceed to execute.

  // --- Case 4: Out of instructions ---
  if (pc >= m_instr.size()) {
    m_state = ProcessState::FINISHED;
    m_metrics.finished_tick = global_tick;
    m_metrics.finish_time = std::time(nullptr);
    return {ProcessState::FINISHED, std::vector<std::string>()};
  }

  // --- Case 5: Execute instruction normally ---
  m_state = ProcessState::RUNNING;
  const Instruction &inst = m_instr[pc];

  switch (inst.type) {
  case InstructionType::PRINT: {
    // PRINT(msg) or PRINT("Hello world from <process_name>!")
    std::string out;
    if (!inst.args.empty()) {
        // Check if arg is a variable
        auto val_opt = read_token_value(inst.args[0]);
        if (!val_opt) {
             m_state = ProcessState::BLOCKED_PAGE_FAULT;
             return {ProcessState::BLOCKED_PAGE_FAULT, {}};
        }
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
    // DECLARE var value
    if (inst.args.size() >= 2) {
      auto val_opt = read_token_value(inst.args[1]);
      if (!val_opt) { m_state = ProcessState::BLOCKED_PAGE_FAULT; return {ProcessState::BLOCKED_PAGE_FAULT, {}}; }

      if (!set_var_value(inst.args[0], *val_opt)) {
          m_state = ProcessState::BLOCKED_PAGE_FAULT;
          return {ProcessState::BLOCKED_PAGE_FAULT, {}};
      }

#ifdef DEBUG_PROCESS
      std::ostringstream dbg;
      dbg << m_name << ": DECLARE " << inst.args[0] << " = " << *val_opt;
      m_logs.push_back(dbg.str());
#endif
    } else if (inst.args.size() == 1) {
       if (!set_var_value(inst.args[0], 0)) {
          m_state = ProcessState::BLOCKED_PAGE_FAULT;
          return {ProcessState::BLOCKED_PAGE_FAULT, {}};
      }
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
      auto a_opt = read_token_value(inst.args[1]);
      if (!a_opt) { m_state = ProcessState::BLOCKED_PAGE_FAULT; return {ProcessState::BLOCKED_PAGE_FAULT, {}}; }

      auto b_opt = read_token_value(inst.args[2]);
      if (!b_opt) { m_state = ProcessState::BLOCKED_PAGE_FAULT; return {ProcessState::BLOCKED_PAGE_FAULT, {}}; }

      uint32_t sum = static_cast<uint32_t>(*a_opt) + static_cast<uint32_t>(*b_opt);
      uint16_t result = clamp_uint16(sum);

      if (!set_var_value(inst.args[0], result)) {
          m_state = ProcessState::BLOCKED_PAGE_FAULT;
          return {ProcessState::BLOCKED_PAGE_FAULT, {}};
      }
#ifdef DEBUG_PROCESS
      std::ostringstream dbg;
      dbg << m_name << ": ADD " << inst.args[0] << " = " << *a_opt << " + " << *b_opt << " = "
          << result << (sum > 65535 ? " (clamped)" : "");
      m_logs.push_back(dbg.str());
#endif
    }
    ++pc;
    break;
  }

  case InstructionType::SUBTRACT: {
    if (inst.args.size() >= 3) {
      auto a_opt = read_token_value(inst.args[1]);
      if (!a_opt) { m_state = ProcessState::BLOCKED_PAGE_FAULT; return {ProcessState::BLOCKED_PAGE_FAULT, {}}; }

      auto b_opt = read_token_value(inst.args[2]);
      if (!b_opt) { m_state = ProcessState::BLOCKED_PAGE_FAULT; return {ProcessState::BLOCKED_PAGE_FAULT, {}}; }

      int32_t a = static_cast<int32_t>(*a_opt);
      int32_t b = static_cast<int32_t>(*b_opt);
      int64_t res = static_cast<int64_t>(a) - static_cast<int64_t>(b);
      uint16_t result = clamp_uint16(res);

      if (!set_var_value(inst.args[0], result)) {
          m_state = ProcessState::BLOCKED_PAGE_FAULT;
          return {ProcessState::BLOCKED_PAGE_FAULT, {}};
      }
#ifdef DEBUG_PROCESS
      std::ostringstream dbg;
      dbg << m_name << ": SUBTRACT " << inst.args[0] << " = " << a << " - " << b << " = "
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


      // Increment executed-instruction count (skip FOR)
      if (inst.type != InstructionType::FOR) {
        ++m_metrics.executed_instructions;
      }

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
        return {ProcessState::FINISHED, {}};
      }
      return {ProcessState::WAITING, {std::to_string(ticks)}};
    }
    break;
  }

  case InstructionType::FOR: {
    // FOR should have been unrolled in constructor. If encountered, skip
    // safely.
    ++pc;
    break;
  }

  case InstructionType::READ: {
      // READ <var> <address>
      if (inst.args.size() >= 2) {
          uint32_t addr = 0;
          try {
              addr = std::stoul(inst.args[1], nullptr, 0); // Handle hex/dec
          } catch (...) {
              // Invalid address format -> treat as 0 or error?
              // For now, let's assume valid format or 0.
              addr = 0;
          }

          // Memory Violation Check
          // We need to check if addr is within valid range?
          // The specs say: "Memory spaces are bound within your running program’s memory address."
          // But we don't have a strict "segment size" per process other than page table size.
          // However, we can check if it maps to a valid page index.
          // Or if it's arbitrarily large.
          // Let's assume max virtual memory is 2^16 (65536) as per specs "All memory ranges are [2^6, 2^16]".
          if (addr >= 65536) {
              // Memory Violation
              m_state = ProcessState::FINISHED; // Terminate
              std::ostringstream err;
              err << "Process " << m_name << " shut down due to memory access violation error that occurred at "
                  << std::put_time(std::localtime(&m_metrics.start_time), "%H:%M:%S") // Approximate time
                  << ". " << "0x" << std::hex << addr << " invalid.";
              m_logs.push_back(err.str());
              std::cout << err.str() << "\n"; // Print to console as per specs
              return {ProcessState::FINISHED, {}};
          }

          auto phys = translate(addr);
          if (!phys) {
              set_faulting_page(addr / m_page_size);
              m_state = ProcessState::BLOCKED_PAGE_FAULT;
              return {ProcessState::BLOCKED_PAGE_FAULT, {}};
          }

          // read_physical already reads 2 bytes (uint16_t)
          uint16_t val = MemoryManager::getInstance().read_physical(phys->first, phys->second);

          if (!set_var_value(inst.args[0], val)) {
               m_state = ProcessState::BLOCKED_PAGE_FAULT;
               return {ProcessState::BLOCKED_PAGE_FAULT, {}};
          }
      }
      ++pc;
      break;
  }

  case InstructionType::WRITE: {
      // WRITE <address> <var>
      if (inst.args.size() >= 2) {
          uint32_t addr = 0;
          try {
              addr = std::stoul(inst.args[0], nullptr, 0);
          } catch (...) { addr = 0; }

          if (addr >= 65536) {
               m_state = ProcessState::FINISHED;
               std::ostringstream err;
               // Get current time
               auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
               err << "Process " << m_name << " shut down due to memory access violation error that occurred at "
                   << std::put_time(std::localtime(&now), "%H:%M:%S")
                   << ". " << "0x" << std::hex << addr << " invalid.";
               m_logs.push_back(err.str());
               std::cout << err.str() << "\n";
               return {ProcessState::FINISHED, {}};
          }

          auto val_opt = read_token_value(inst.args[1]);
          if (!val_opt) {
              m_state = ProcessState::BLOCKED_PAGE_FAULT;
              return {ProcessState::BLOCKED_PAGE_FAULT, {}};
          }

          auto phys = translate(addr);
          if (!phys) {
              set_faulting_page(addr / m_page_size);
              m_state = ProcessState::BLOCKED_PAGE_FAULT;
              return {ProcessState::BLOCKED_PAGE_FAULT, {}};
          }

          // write_physical already writes 2 bytes (uint16_t)
          MemoryManager::getInstance().write_physical(phys->first, phys->second, *val_opt);
      }
      ++pc;
      break;
  }

  default:
    // Unknown instruction: skip
    ++pc;
    break;
  }

  // Increment executed-instruction count (skip FOR)
  if (inst.type != InstructionType::FOR) {
    ++m_metrics.executed_instructions;
  }

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
    return {ProcessState::FINISHED, {}};
  }

  return {ProcessState::RUNNING, {}};
}

Process::MemoryStats Process::get_memory_stats() const {
    std::lock_guard<std::mutex> lk(m_mutex);
    size_t active = 0;
    size_t swap = 0;
    for (const auto& page : page_table_) {
        if (page.valid) active++;
        if (page.on_disk) swap++;
    }
    return {active, swap, page_table_.size()};
}
// TODO: Consider renaming this not to conflict with other read_token_value.
std::optional<uint16_t> Process::read_token_value(const std::string &token) {
  if (is_number(token)) {
    try {
      return clamp_uint16(std::stoll(token));
    } catch (...) {
      return 0;
    }
  }

  auto it = symbol_table_.find(token);
  if (it == symbol_table_.end()) {
      // Variable not found, create it
      size_t v_addr = current_brk_;
      size_t page_idx = v_addr / m_page_size;
      if (page_idx >= page_table_.size()) {
          page_table_.resize(page_idx + 1);
      }

      symbol_table_[token] = v_addr;
      current_brk_ += 2; // uint16_t size
      return 0;
  }

  size_t v_addr = it->second;
  auto phys = translate(v_addr);
  if (!phys) {
      set_faulting_page(v_addr / m_page_size);
      return std::nullopt; // Page fault
  }

    // Read 2 bytes (uint16_t) directly
    return MemoryManager::getInstance().read_physical(phys->first, phys->second);
}

bool Process::set_var_value(const std::string &name, uint16_t v) {
    auto it = symbol_table_.find(name);
    if (it == symbol_table_.end()) {
        // Create variable
        size_t v_addr = current_brk_;
        size_t page_idx = v_addr / m_page_size;
        if (page_idx >= page_table_.size()) {
            page_table_.resize(page_idx + 1);
        }
        symbol_table_[name] = v_addr;
        current_brk_ += 2;
    }

    size_t v_addr = symbol_table_[name];
    auto phys = translate(v_addr);
    if (!phys) {
        set_faulting_page(v_addr / m_page_size);
        return false; // Page fault
    }

    // Write 2 bytes (uint16_t) directly
    MemoryManager::getInstance().write_physical(phys->first, phys->second, v);

    return true;
}

std::optional<std::pair<size_t, size_t>> Process::translate(size_t v_addr) {
    size_t page_num = v_addr / m_page_size;
    size_t offset = v_addr % m_page_size;

    if (page_num >= page_table_.size()) return std::nullopt;
    if (!page_table_[page_num].valid) return std::nullopt;

    return std::make_pair(page_table_[page_num].frame_idx, offset);
}

void Process::update_page_table(size_t page_num, size_t frame_idx) {
    if (page_num >= page_table_.size()) {
        page_table_.resize(page_num + 1);
    }
    page_table_[page_num].frame_idx = frame_idx;
    page_table_[page_num].valid = true;
    page_table_[page_num].on_disk = false;
}

void Process::invalidate_page(size_t page_num) {
    if (page_num < page_table_.size()) {
        page_table_[page_num].valid = false;
        page_table_[page_num].on_disk = true;
    }
}

void Process::initialize_memory(size_t mem_size, size_t page_size) {
    m_page_size = page_size;
    size_t num_pages = (mem_size + page_size - 1) / page_size;
    page_table_.resize(num_pages);
    current_brk_ = 0;
}

bool Process::is_page_on_disk(size_t page_num) const {
    if (page_num >= page_table_.size()) return false;
    return page_table_[page_num].on_disk;
}
