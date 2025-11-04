#include "../include/cli.hpp"
#include <iostream>
#include <string>
#include <chrono>
#include <thread>

// cool typewriter thingy
static void slow_print(const std::string &text, int delay_ms = 0) {
  for (char c : text) {
    std::cout << c << std::flush;
    if (delay_ms > 0)
      std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
  }
}

// Displays the project banner once before the CLI starts
static void print_welcome_banner() {
  const std::string banner = R"(
=============================================
         PROCESS SCHEDULER EMULATOR
=============================================
Developed by:
  Bunyi, Christian Joseph C.
  Campo, Roan Cedric V.
  Chan, Enzo Rafael S.
  Dellosa, Mariella Jeanne A.
Section: CSOPESY-S13
---------------------------------------------
System initializing... please wait.
)";

  slow_print(banner, 0); // typing delay
  std::cout << std::endl;
}

// CLI owns no CPUWorker threads
// Scheduler manages workers

static void print_prompt() {
  std::cout << "csopesy> ";
  std::cout.flush();
}

CLI::CLI() {
  scheduler_ = new Scheduler(cfg_);
  generator_ = new ProcessGenerator(cfg_, *scheduler_);
  reporter_  = new Reporter(*scheduler_);
}


CLI::~CLI() {
  // stop long-running components first, then delete in any order
  if (generator_) generator_->stop();
  if (scheduler_) scheduler_->stop();

  // delete and null each pointer explicitly
  if (generator_) { delete generator_; generator_ = nullptr; }
  if (scheduler_) { delete scheduler_; scheduler_ = nullptr; }
  if (reporter_)  { delete reporter_;  reporter_  = nullptr; }
}

int CLI::run() {

  if (initialized_)
    return 1;
    
  initialized_ = true;

  print_welcome_banner();

  scheduler_->start();
  generator_->start();

  std::string line;
  print_prompt();

  while (std::getline(std::cin, line)) {

    if (line.empty()) {
      // ignore empty input
    }

    else if (line == "exit" || line == "quit") {
      break;
    }

    else if (line == "initialize") {
      std::cout << "System initialized.\n";
    }

    else if (line == "scheduler-start") {
      std::cout << "Scheduler: process generation started.\n";
      // generator_ already started in setup
    }

    else if (line == "report-util" || line == "screen -ls") {
      std::cout << reporter_->build_report() << '\n';
    }

    else {
      std::cout << "Unknown command: " << line << '\n';
    }

    print_prompt();
  }

  return 0;
}

void CLI::handle_command(const std::string &cmd) {
  (void)cmd; // optional
}
