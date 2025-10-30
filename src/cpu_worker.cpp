#include "../include/cpu_worker.hpp"

CPUWorker::CPUWorker(uint32_t id, Scheduler &sched) : id_(id), sched_(sched) {}

void CPUWorker::start() {
  if (running_.load())
    return;
  running_.store(true);
  thread_ = std::thread(&CPUWorker::loop, this);
}

void CPUWorker::stop() { running_.store(false); }

void CPUWorker::join() {
  if (thread_.joinable()) {
    thread_.join();
  }
}

void CPUWorker::loop() {
  // TODO: Implement worker loop
  while (running_.load()) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
}
