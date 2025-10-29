#pragma once
#include "instruction.hpp"
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

enum class ProcessState { NEW, READY, RUNNING, WAITING, FINISHED };

struct ProcessMetrics {
  uint32_t created_tick{0};
  uint32_t finished_tick{0};
};

class Process {
public:
  Process(uint32_t id, const std::string &name, std::vector<Instruction> ins);
  uint32_t id() const;
  std::string name() const;
  ProcessState state();
  void set_state(ProcessState s);
  // Execution API used by CPUWorker
  // Returns true if finished after this tick's execution
  bool execute_tick(uint32_t global_tick, uint32_t delays_per_exec,
                    uint32_t &consumed_ticks);
  // Accessors
  std::vector<std::string> get_logs(); // thread-safe snapshot
  std::string smi_summary();           // formatted status + logs
  // Visible members:
  uint32_t pc{0};
  std::unordered_map<std::string, uint16_t> vars;

private:
  uint32_t m_id;
  std::string m_name;
  std::vector<Instruction> m_instr;
  ProcessState m_state;
  std::vector<std::string> m_logs;
  std::mutex m_mutex; // protects state, logs, vars, pc
  // runtime helpers:
  uint32_t m_sleep_remaining{0};
  uint32_t m_for_stack_depth{0};
  ProcessMetrics m_metrics;
};
