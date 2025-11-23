#include <vector>
#include <iostream>
#include "../include/data_structures/timer_entry.hpp"

std::vector<TimerEntry> TimerEntrySleepQueue::get_sleep_queue_snapshot() const {
    std::lock_guard<std::mutex> lock(sleep_queue_mtx_);

    // Make a shallow copy of the priority_queue’s contents safely
    auto copy = sleep_queue_;
    std::vector<TimerEntry> snapshot;
    while (!copy.empty()) {
        snapshot.push_back(copy.top());
        copy.pop();
    }
    return snapshot;
}

std::string TimerEntrySleepQueue::print_sleep_queue() const {
    std::lock_guard<std::mutex> lock(sleep_queue_mtx_);
    std::priority_queue<TimerEntry> copy = sleep_queue_;
    std::ostringstream oss;
    while (!copy.empty()) {
        const auto &t = copy.top();
        if (t.process)
            oss << t.process->name()
                << "\tPID:" << t.process->id() << "\tWT " << t.wake_tick << "\n";
        else
            oss << "  [NULL process] wakes at " << t.wake_tick << "\n";
        copy.pop();
    }
    return oss.str();
}

void TimerEntrySleepQueue::send(std::shared_ptr<Process> p, uint64_t wake_tick) {
    if (!p) {
        std::cerr << "[ERROR] Tried to queue null process for sleep\n";
        return;
    }
    // Adding a sleeping process
    std::lock_guard<std::mutex> lock(sleep_queue_mtx_);
    TimerEntry t{p, wake_tick};
    sleep_queue_.push(t);
}

TimerEntry TimerEntrySleepQueue::receive() {
    std::lock_guard<std::mutex> lock(sleep_queue_mtx_);
    if (sleep_queue_.empty()) {
        return TimerEntry{nullptr, 0};
    }
    TimerEntry t = sleep_queue_.top();
    sleep_queue_.pop();
    return t;
}

bool TimerEntrySleepQueue::isEmpty() const {
    std::lock_guard<std::mutex> lock(sleep_queue_mtx_);
    return sleep_queue_.empty();
}

TimerEntry TimerEntrySleepQueue::top() const {
    std::lock_guard<std::mutex> lock(sleep_queue_mtx_);
    if (sleep_queue_.empty()) {
        return TimerEntry{nullptr, 0};
    }
    return sleep_queue_.top();
}
