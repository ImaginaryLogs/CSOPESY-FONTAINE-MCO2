#pragma once
#include "processes/instruction.hpp"
#include <ctime>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>
#include <optional>

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

struct ProcessReturnContext{
  ProcessState state;
  std::vector<std::string> args;
};

bool is_yielded(ProcessReturnContext context) noexcept;

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
  bool finished_logged = false;

  // === Metadata accessors ===
  std::vector<std::string> get_logs(); // thread-safe snapshot
  std::string smi_summary();           // formatted status + logs
  std::string summary_line(bool colorize = false) const; // for screen -ls

  // === Scheduler metadata ===
  uint32_t priority{0};         // process priority
  uint32_t ticks_waited{0};     // for aging or fairness
  uint32_t last_active_tick{0}; // for LRU / victim selection
  uint32_t cpu_id{256};         // which CPU last ran it

  // === Paging Support ===
  uint32_t pc{0};                                 // program counter
  std::unordered_map<std::string, uint16_t> vars; // memory storage
  struct PageEntry {
    size_t frame_idx{0};
    bool valid{false};   // In RAM
    bool on_disk{false}; // In Backing Store
    bool dirty{false};   // Modified (tracked here or in MemoryManager? Let's track here too for consistency if needed, but MM handles eviction)
  };

  // Called by Scheduler to update page table after handling fault
  void update_page_table(size_t page_num, size_t frame_idx);

  // Called by Scheduler to invalidate a page (eviction)
  void invalidate_page(size_t page_num);

  // Check if page is in backing store
  bool is_page_on_disk(size_t page_num) const;

  // Helper to translate and check validity
  // Returns {frame_idx, offset} if valid
  // Returns nullopt if page fault (caller should return BLOCKED_PAGE_FAULT)
  std::optional<std::pair<size_t, size_t>> translate(size_t v_addr);

  // Memory limits
  size_t memory_limit_{0};
  size_t current_brk_{0}; // Allocator pointer

  // Page Table: Index = Page Number
  std::vector<PageEntry> page_table_;

  // Symbol Table: Variable Name -> Virtual Address
  std::unordered_map<std::string, uint32_t> symbol_table_;

  // Removed old vars map
  // std::unordered_map<std::string, uint16_t> vars;

  // === Execution control ===
  void set_state(ProcessState s);
  std::string get_state_string();
  void set_core_id(uint32_t core);
  uint32_t get_core_id() const;
  void set_name(std::string);
  ProcessState get_state();
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
  ProcessReturnContext execute_tick(uint32_t global_tick, uint32_t delays_per_exec,
                            uint32_t &consumed_ticks);

  // Initialize memory (called by Scheduler on startup)
  void initialize_memory(size_t mem_size, size_t mem_per_frame);

  size_t get_faulting_page() const { return last_fault_page_; }
  void set_faulting_page(size_t page) { last_fault_page_ = page; }

  struct MemoryStats {
      size_t active_pages;
      size_t swap_pages;
      size_t total_pages;
  };
  MemoryStats get_memory_stats() const;

private:
  std::optional<uint16_t> read_token_value(const std::string &token);
  bool set_var_value(const std::string &name, uint16_t v);

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

  // Config copy for memory calculations (page size)
  size_t m_page_size{16}; // Default, updated in initialize_memory
  size_t last_fault_page_{0};
};
