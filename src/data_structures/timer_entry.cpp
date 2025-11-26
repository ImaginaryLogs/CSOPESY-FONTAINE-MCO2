#include <vector>
#include <iostream>
#include <format>
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

std::string TimerEntrySleepQueue::snapshot() const {
    std::lock_guard<std::mutex> lock(sleep_queue_mtx_);
    std::priority_queue<TimerEntry, std::vector<TimerEntry>, std::greater<TimerEntry>> copy = sleep_queue_;
    std::ostringstream oss;
    uint16_t ui_showlimit = 10;
    uint16_t counter = 0;
    auto top = copy.size();
    if (!copy.empty())
        oss << "Tick\tName\tPID\t#\n"
            << "------------------------------\n";
    while (!copy.empty()) {
        const auto &t = copy.top();
        if (t.process)
            oss << t.wake_tick << "t\t"
                << t.process->name() << "\t"
                << t.process->id() << "\t"
                << top << "\n";
        else
            oss << "  [NULL process] wakes at " << t.wake_tick << "\n";
        copy.pop();
        counter++;
        top--;
        if (counter >= ui_showlimit)
            break;
        
    }

    oss << ((counter > ui_showlimit) ? std::format("... and {} more \n", sleep_queue_.size() - ui_showlimit) : "\n");
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

size_t TimerEntrySleepQueue::size() const {
    return sleep_queue_.size();
}

std::string TimerEntrySleepQueue::print() const {
    std::lock_guard<std::mutex> lock(sleep_queue_mtx_);
    std::priority_queue<TimerEntry, std::vector<TimerEntry>, std::greater<TimerEntry>> copy = sleep_queue_;
    std::ostringstream oss;
    uint16_t ui_showlimit = 10;
    auto top = copy.size();
    if (!copy.empty())
        oss << "Tick\tName\tPID\t#\n"
            << "------------------------------\n";
    while (!copy.empty()) {
        const auto &t = copy.top();
        if (t.process)
            oss << t.wake_tick << "t\t"
                << t.process->name() << "\t"
                << t.process->id() << "\t"
                << top << "\n";
        else
            oss << "  [NULL process] wakes at " << t.wake_tick << "\n";
        copy.pop();
        top--;
    }

    oss << "\n";
    return oss.str();
}
