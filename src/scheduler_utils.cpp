#include "../include/scheduler.hpp"
#include "../include/process.hpp"

// This files just contains Scheduler's Utility functions

void Scheduler::InitializeQueues() {
  this->ready_queue_ = std::deque<std::shared_ptr<Process>>();
  this->job_queue_ = std::deque<std::shared_ptr<Process>>();
  this->blocked_queue_ = std::deque<std::shared_ptr<Process>>();
  this->swapped_queue_ = std::deque<std::shared_ptr<Process>>();
}

void Scheduler::InitializeVectors() {
  this->running_ = std::vector<std::shared_ptr<Process>>(cfg_.num_cpu, nullptr);
  this->finished_ = std::vector<std::shared_ptr<Process>>(cfg_.num_cpu, nullptr);
}