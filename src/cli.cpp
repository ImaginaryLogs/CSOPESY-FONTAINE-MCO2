#include "../include/cli.hpp"
#include "../include/process.hpp"
#include "../include/scheduler.hpp"
#include <iostream>
#include <sstream>
#include <vector>
#include <cctype>
#include <algorithm>
#include <memory>   // <-- needed for std::shared_ptr

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
      break;
    }
    else if (cmd == "initialize") {
      initialize_system();
    }
    else if (cmd == "scheduler-start") {
      if (require_init()) { generator_->start(); std::cout << "Process generator started.\n"; }
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
    else {
      std::cout << "Unknown command: " << line << "\n";
    }

    prompt();
  }

  return 0;
}

void CLI::handle_command(const std::string &cmd) { (void)cmd; }
