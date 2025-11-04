#pragma once
#include "instruction.hpp"
#include <ctime>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

enum class ProcessState {
  // Long Term
  NEW,
  // Medium Term
  WAITING,
  BLOCKED_PAGE_FAULT,
  SWAPPED_OUT,
  FINISHED,
  // Short Term
  READY,
  RUNNING,
};

/**
 * Runtime metrics for process execution
 * Extended to include instruction counts, timing, and CPU affinity.
 */
struct ProcessMetrics {
  uint32_t created_tick{0};
  uint32_t finished_tick{0};
  uint32_t executed_instructions{0};
  uint32_t total_instructions{0};
  uint32_t core_id{UINT32_MAX}; // Which CPU core executed this process
  std::time_t start_time{0};
  std::time_t finish_time{0};
};

/**
 * Represents a process executing a sequence of instructions.
 * Handles its own instruction set, internal variables, and execution tick
 * cycle.
 */
class Process {
public:
  Process(uint32_t id, const std::string &name, std::vector<Instruction> ins);

  uint32_t id() const;
  std::string name() const;
  ProcessState state();

  // === Metadata accessors ===
  std::vector<std::string> get_logs(); // thread-safe snapshot
  std::string smi_summary();           // formatted status + logs
  std::string summary_line(bool colorize = false) const; // for screen -ls

  // === Scheduler metadata ===
  uint32_t priority{0};         // process priority
  uint32_t ticks_waited{0};     // for aging or fairness
  uint32_t last_active_tick{0}; // for LRU / victim selection
  uint32_t cpu_id{256};         // which CPU last ran it

  // === Program Related Members ===
  uint32_t pc{0};                                 // program counter
  std::unordered_map<std::string, uint16_t> vars; // memory storage

  // === Execution control ===
  void set_state(ProcessState s);
  std::string get_state_string();
  void set_core_id(uint32_t core);
  uint32_t get_core_id() const;

  // === Helpers ===
  uint32_t get_total_instructions() const;
  uint32_t get_executed_instructions() const;
  uint32_t get_remaining_sleep_ticks() const;

  // === Sleep Helpers ===
  void set_sleep_ticks(uint32_t ticks);
  void clear_sleep();

  // === State Query Helpers ===
  bool is_new() const noexcept;
  bool is_ready() const noexcept;
  bool is_running() const noexcept;
  bool is_waiting() const noexcept;
  bool is_finished() const noexcept;
  bool is_swapped() const noexcept;
  bool is_blocked() const noexcept;

  // === State Transition Helpers ===
  void mark_ready();
  void mark_running();
  void mark_waiting();
  void mark_swapped();
  void mark_finished(uint32_t tick);

  bool has_instructions_remaining() const noexcept;

  // Execution API used by CPUWorker
  // Returns the state of the process after this tick's execution
  ProcessState execute_tick(uint32_t global_tick, uint32_t delays_per_exec,
                            uint32_t &consumed_ticks);

private:
  uint32_t m_id;
  std::string m_name;
  std::vector<Instruction> m_instr;
  ProcessState m_state{ProcessState::NEW};
  std::vector<std::string> m_logs;
  mutable std::mutex m_mutex; // protects state, logs, vars, pc

  // runtime helpers
  uint32_t m_delay_remaining{0};
  uint32_t m_sleep_remaining{0};
  uint32_t m_for_stack_depth{0};
  ProcessMetrics m_metrics;
};
