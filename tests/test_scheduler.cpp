#include "../include/scheduler.hpp"
#include "../include/config.hpp"
#include "../include/process.hpp"
#include "../include/cpu_worker.hpp"
#include <thread>
#include <chrono>
#include <iostream>

int main()
{
  std::cout << "Starting Scheduler pause/resume test...\n";
  {
    Config cfg;
    cfg.num_cpu = 2;
    cfg.scheduler_tick_delay = 100;

    Scheduler sched(cfg);
    sched.start();
    std::cout << "Starting." << "\n";
    std::cout << sched.snapshot() << "\n";
    std::this_thread::sleep_for(std::chrono::seconds(2));

    sched.pause();
    std::cout << "Paused at tick: " << sched.current_tick() << "\n";  
    std::cout << sched.snapshot() << "\n";
    std::this_thread::sleep_for(std::chrono::seconds(2));

    sched.resume();
    std::cout << "Resumed.\n";
    std::cout << sched.snapshot() << "\n";
    std::this_thread::sleep_for(std::chrono::seconds(2));
    std::cout << "Stopping scheduler...\n";
    sched.stop();
    std::cout << "Stopped safely.\n";
  }
}