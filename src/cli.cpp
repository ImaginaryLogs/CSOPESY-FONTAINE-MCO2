#include "../include/cli.hpp"

/**
 * NOTE:
 * This is just a skeleton that CJ created to get things going.
 * Feel free to add/remove/revise anything.
 */

CLI::CLI() {
  // Initialize core components
  scheduler_ = new Scheduler(cfg_);
  generator_ = new ProcessGenerator(cfg_, *scheduler_);
  reporter_ = new Reporter(*scheduler_);

  // Create CPU workers
  cpu_workers_.reserve(cfg_.num_cpu);
  for (uint32_t i = 0; i < cfg_.num_cpu; ++i) {
    cpu_workers_.push_back(std::make_unique<CPUWorker>(i, *scheduler_));
  }
}

CLI::~CLI() {
  if (generator_) {
    generator_->stop();
    delete generator_;
  }

  for (auto &worker : cpu_workers_) {
    if (worker) {
      worker->stop();
      worker->join();
    }
  }

  if (scheduler_) {
    scheduler_->stop();
    delete scheduler_;
  }

  if (reporter_) {
    delete reporter_;
  }
}

int CLI::run() {
  if (initialized_)
    return 1;

  initialized_ = true;

  // Start all components
  scheduler_->start();
  for (auto &worker : cpu_workers_) {
    worker->start();
  }
  generator_->start();

  // TODO: Implement command loop
  return 0;
}

void CLI::handle_command(const std::string &cmd) {
  // TODO: Implement command handling
}
