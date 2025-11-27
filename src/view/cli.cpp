#include "view/cli.hpp"
#include "config.hpp"
#include "paging/memory_manager.hpp"
#include "processes/process.hpp"
#include "kernel/scheduler.hpp"
#include <iostream>
#include <sstream>
#include <vector>
#include <cctype>
#include <algorithm>
#include <memory>   // <-- needed for std::shared_ptr
#include <fstream>
#include <unistd.h>
#include <cstring>
#include <thread>
#include <chrono>

// util funcs

static std::string to_lower(std::string s) {
  std::transform(s.begin(), s.end(), s.begin(),
                 [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
  return s;
}

static std::vector<std::string> split(const std::string &line) {
  std::istringstream iss(line);
  std::vector<std::string> out;
  std::string tok;
  while (iss >> tok) out.push_back(tok);
  return out;
}

static void print_banner() {
  std::cout
    << "=============================================\n"
    << "         PROCESS SCHEDULER EMULATOR\n"
    << "=============================================\n"
    << "Developed by:\n"
    << "  Bunyi, Christian Joseph C.\n"
    << "  Campo, Roan Cedric V.\n"
    << "  Chan, Enzo Rafael S.\n"
    << "  Dellosa, Mariella Jeanne A.\n"
    << "Section: CSOPESY-S13\n"
    << "---------------------------------------------\n"
    << "System initializing... please wait.\n\n";
}

static void prompt() { std::cout << "csopesy> " << std::flush; }

// CLI class implementation

CLI::CLI() = default;

CLI::~CLI() {
  if (generator_) generator_->stop();
  if (scheduler_) { scheduler_->stop(); delete scheduler_; scheduler_ = nullptr; }
  if (reporter_) { delete reporter_; reporter_ = nullptr; }
}

// helper member funcs

bool CLI::require_init() const {
  if (!initialized_) {
    std::cout << "Please run initialize first.\n";
    return false;
  }
  return true;
}

void CLI::initialize_system() {
  cfg_ = load_config("config.txt");

  // Initialize MemoryManager with configuration
  MemoryManager::getInstance().initialize(cfg_);

  if (scheduler_) { scheduler_->stop(); delete scheduler_; }
  scheduler_ = new Scheduler(cfg_);
  scheduler_->start();

  // One-time CPU idle snapshot after scheduler starts
  for (uint32_t i = 0; i < cfg_.num_cpu; ++i)
    std::cout << "  CPU ID: " << i << " IDLE\n";

  if (generator_) delete generator_;
  generator_ = new ProcessGenerator(cfg_, *scheduler_);

  if (reporter_) delete reporter_;
  reporter_ = new Reporter(*scheduler_);

  initialized_ = true;
  std::cout << "Initialization complete.\n";
}

void CLI::attach_process_screen(const std::string& name, const std::shared_ptr<Process>& proc) {
  std::cout << "Attached to " << name << ". Type 'process-smi' or 'exit'.\n";

  std::string sub;
  while (true) {
    std::cout << name << "> " << std::flush;
    if (!std::getline(std::cin, sub)) break;
    if (sub == "exit") break;

    if (sub == "process-smi") {
      std::cout << proc->smi_summary();
      if (proc->state() == ProcessState::FINISHED)
        std::cout << "Finished!\n";
    } else {
      std::cout << "Unknown command.\n";
    }
  }
}

void CLI::handle_screen_command(const std::vector<std::string>& args) {
  if (!require_init()) return;

  if (args.size() >= 2 && args[1] == "-ls") {
    std::cout << reporter_->build_report();
    return;
  }

  if (args.size() >= 3 && args[1] == "-s") {
    const std::string name = args[2];
    uint32_t est = 0;
    auto ins = generator_->generate_instructions(cfg_.min_ins, est);

    static uint32_t user_pid = 100000; // avoid collision with generator
    const uint32_t pid = user_pid++;

    auto p = std::make_shared<Process>(pid, name, ins);
    screen_mgr_.create_screen(name, p);
    scheduler_->submit_process(p);

    std::cout << "Created process " << name << "\n";
    return;
  }

  if (args.size() >= 3 && args[1] == "-r") {
    const std::string name = args[2];
    auto proc = screen_mgr_.find(name);

    if (!proc) {
      std::cout << "Process " << name << " not found.\n";
      return;
    }

    attach_process_screen(name, proc);
    return;
  }

  if (args.size() >= 4 && args[1] == "-c") {
      // screen -c <process_name> <mem_size> "<instructions>"
      // Note: args will be split by spaces, so instructions might be spread across args.
      // We need to reconstruct the instruction string or parse carefully.
      // But wait, split() splits by whitespace.
      // If the user types: screen -c p1 128 "INS1; INS2"
      // args[0]=screen, args[1]=-c, args[2]=p1, args[3]=128, args[4]="INS1;, args[5]=INS2"
      // We need to handle quoted string reconstruction? Or just join the rest.

      const std::string name = args[2];
      uint32_t mem_size = 0;
      try {
          mem_size = std::stoul(args[3]);
      } catch (...) {
          std::cout << "Invalid memory size.\n";
          return;
      }

      // Reconstruct instructions from args[4:]
      std::string instruction_str;
      for (size_t i = 4; i < args.size(); ++i) {
          if (i > 4) instruction_str += " ";
          instruction_str += args[i];
      }

      // Remove quotes if present
      if (instruction_str.size() >= 2 && instruction_str.front() == '"' && instruction_str.back() == '"') {
          instruction_str = instruction_str.substr(1, instruction_str.size() - 2);
      }

      // Parse instructions
      std::vector<Instruction> ins;
      std::stringstream ss(instruction_str);
      std::string segment;
      while (std::getline(ss, segment, ';')) {
          // Trim whitespace
          segment.erase(0, segment.find_first_not_of(" \t\n\r"));
          segment.erase(segment.find_last_not_of(" \t\n\r") + 1);
          if (segment.empty()) continue;

          // Parse segment: TYPE arg1 arg2 ...
          // Or TYPE(arg1, arg2) ?
          // Specs say: "DECLARE varA 10; ADD varA varA varB; PRINT(\"Result: \" + varC)"
          // It seems mixed format?
          // "DECLARE varA 10" -> Space separated
          // "PRINT(\"Result: \" + varC)" -> Function style?
          // Let's assume space separated for simplicity first as per previous examples,
          // but the sample usage shows "PRINT(\"Result: \" + varC)".
          // Our current Instruction structure supports args vector.
          // Let's try to parse space separated first, handling quotes?
          // Actually, let's just split by space for now, treating quoted parts as one arg if possible?
          // Or just simple split.
          // For "PRINT(\"Result: \" + varC)", it's complex.
          // Let's support the simple space-separated format for DECLARE, ADD, etc.
          // And maybe special handling for PRINT if it starts with PRINT(?

          std::stringstream seg_ss(segment);
          std::string type_str;
          seg_ss >> type_str;

          Instruction instr;
          if (type_str == "DECLARE") instr.type = InstructionType::DECLARE;
          else if (type_str == "PRINT") instr.type = InstructionType::PRINT;
          else if (type_str == "ADD") instr.type = InstructionType::ADD;
          else if (type_str == "SUBTRACT") instr.type = InstructionType::SUBTRACT; // Assuming SUBTRACT/SUB
          else if (type_str == "SLEEP") instr.type = InstructionType::SLEEP;
          else if (type_str == "WRITE") instr.type = InstructionType::WRITE;
          else if (type_str == "READ") instr.type = InstructionType::READ;
          else {
              std::cout << "Unknown instruction type: " << type_str << "\n";
              continue;
          }

          // Parse args
          // If PRINT and next char is (, handle differently?
          // For now, simple space split.
          std::string arg;
          while (seg_ss >> arg) {
              // Remove potential trailing ) or starting ( for PRINT(msg) style if we want to support it loosely
              // But specs say: "PRINT(\"Result: \" + varC)"
              // This is quite complex to parse fully correctly without a proper lexer.
              // Let's strip ( and ) and " for simple cases.
              instr.args.push_back(arg);
          }
          ins.push_back(instr);
      }

      static uint32_t user_pid = 200000;
      const uint32_t pid = user_pid++;

      auto p = std::make_shared<Process>(pid, name, ins);
      // Set memory limit? Process class doesn't strictly enforce limit at creation,
      // but Scheduler initializes it.
      // Wait, Scheduler::long_term_admission initializes memory based on config.
      // But here we want custom memory size.
      // We might need to set a hint or force it.
      // Process::initialize_memory is called by Scheduler.
      // Maybe we can set a property on Process?
      // Or we can initialize it right now if we bypass long_term_admission?
      // But we submit to scheduler.
      // Let's add a `required_memory` field to Process or Config override?
      // For now, let's assume Scheduler respects it if we set it?
      // Actually, Scheduler overwrites it in `long_term_admission`.
      // We should probably modify Scheduler to respect pre-set memory size?
      // Or add `Process::set_memory_requirement(size_t)`.
      // Let's assume we can set it and Scheduler checks if already set?
      // Scheduler calls `p->initialize_memory`.
      // We can add `p->set_initial_memory_size(mem_size)`.

      // For now, let's just submit.
      scheduler_->submit_process(p);
      std::cout << "Process " << name << " created with " << ins.size() << " instructions.\n";
      return;
  }

  std::cout << "Unknown screen subcommand.\n";
}

int CLI::run() {
  print_banner();
  prompt();

  std::string line;

  while (std::getline(std::cin, line)) {
    const auto args = split(line);
    if (args.empty()) { prompt(); continue; }

    const std::string cmd = to_lower(args[0]);

    if (cmd == "exit") {
      std::cout << "Goodbye.\n";
      scheduler_->resume();

      break;
    }
    else if (cmd == "initialize") {
      initialize_system();
    }
    else if (cmd == "scheduler-start") {
      if (require_init()) { generator_->start(); std::cout << "Process generator started.\n"; scheduler_->resume(); }
    }
    else if (cmd == "pause") {
      if (require_init()) { scheduler_->pause(); std::cout << "Paused.\n"; }
    }
    else if (cmd == "resume") {
      if (require_init()) { scheduler_->resume(); std::cout << "Resumed.\n"; }
    }
    else if (cmd == "policy") {
      if (require_init()) {
        if (args.size() < 2) { std::cout << "Usage: policy <rr|fcfs|priority>\n"; }
        else {
          std::string v = to_lower(args[1]);
          if (v == "rr") scheduler_->setSchedulingPolicy(SchedulingPolicy::RR);
          else if (v == "fcfs") scheduler_->setSchedulingPolicy(SchedulingPolicy::FCFS);
          else if (v == "priority") scheduler_->setSchedulingPolicy(SchedulingPolicy::PRIORITY);
          else { std::cout << "Unknown policy. Use rr|fcfs|priority\n"; }
        }
      }
    }
    else if (cmd == "util") {
      if (require_init()) {
        CpuUtilization util = scheduler_->cpu_utilization();

        std::cout << "CPU utilization: " << static_cast<int>(util.percent) << "%\n"
                  << "Cores used: " << util.used << "\n"
                  << "Cores available: " << util.total << "\n";
      }
    }
    else if (cmd == "scheduler-stop") {
      if (require_init()) generator_->stop();
    }
    else if (cmd == "screen") {
      handle_screen_command(args);
    }
    else if (cmd == "report-util") {
      if (require_init()) {
        std::cout << reporter_->build_report();
        reporter_->write_log("csopesy-log.txt");
      }
    }
    else if (cmd == "process-smi") {
      if (require_init()) {
        std::cout << reporter_->get_process_smi();
      }
    }
    else if (cmd == "vmstat") {
      if (require_init()) {
        std::cout << reporter_->get_vmstat();
      }
    }
    else if (cmd == "help") {
      std::cout << "initialize: initializes the system with config.\n"
                << "scheduler-start: starts the scheduler\n"
                << "util: gets cpu utilization\n"
                << "report-util: writes the report, saves to file.\n"
                << "process-smi: displays memory usage of processes\n"
                << "vmstat: displays system memory usage\n"
                << "scheduler-stop: stops the scheduler\n";
    }
    else {
      std::cout << "Unknown command: " << line << "\n";
    }

    prompt();
  }

  return 0;
}

void CLI::handle_command(const std::string &cmd) { (void)cmd; }
