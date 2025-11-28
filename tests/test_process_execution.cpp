#include "../include/processes/process.hpp"
#include "../include/paging/memory_manager.hpp"
#include "../include/config.hpp"
#include <iostream>
#include <cassert>
#include <vector>
#include <sstream>

// Mock Scheduler/MemoryManager environment if needed, or just test Process logic
// Process::execute_tick relies on MemoryManager singleton.

void test_memory_violation_read() {
    std::cout << "Running test_memory_violation_read..." << std::endl;

    // Setup MemoryManager
    Config cfg;
    cfg.max_overall_mem = 64;
    cfg.mem_per_frame = 16;
    MemoryManager::getInstance().initialize(cfg);

    // Create process with READ instruction accessing invalid address
    std::vector<Instruction> insts = {
        {InstructionType::READ, {"varA", "0x10000"}} // 65536 -> Invalid
    };
    auto p = std::make_shared<Process>(1, "test_proc", insts);
    p->initialize_memory(32, 16); // 2 pages

    // Execute tick
    uint32_t consumed = 0;
    auto res = p->execute_tick(1, 10, consumed);

    assert(res.state == ProcessState::FINISHED);
    assert(p->get_state() == ProcessState::FINISHED);

    // Check logs
    auto logs = p->get_logs();
    bool found_error = false;
    for (const auto& log : logs) {
        if (log.find("memory access violation") != std::string::npos &&
            log.find("0x10000 invalid") != std::string::npos) {
            found_error = true;
            break;
        }
    }
    assert(found_error);
    std::cout << "test_memory_violation_read PASSED" << std::endl;
}

void test_memory_violation_write() {
    std::cout << "Running test_memory_violation_write..." << std::endl;

    // Setup MemoryManager
    Config cfg;
    cfg.max_overall_mem = 64;
    cfg.mem_per_frame = 16;
    MemoryManager::getInstance().initialize(cfg);

    // Create process with WRITE instruction accessing invalid address
    std::vector<Instruction> insts = {
        {InstructionType::WRITE, {"0x10000", "123"}} // 65536 -> Invalid
    };
    auto p = std::make_shared<Process>(2, "test_proc_write", insts);
    p->initialize_memory(32, 16);

    // Execute tick (WRITE)
    uint32_t consumed = 0;
    auto res = p->execute_tick(1, 10, consumed);

    assert(res.state == ProcessState::FINISHED);

    // Check logs
    auto logs = p->get_logs();
    bool found_error = false;
    for (const auto& log : logs) {
        if (log.find("memory access violation") != std::string::npos &&
            log.find("0x10000 invalid") != std::string::npos) {
            found_error = true;
            break;
        }
    }
    assert(found_error);
    std::cout << "test_memory_violation_write PASSED" << std::endl;
}

void test_custom_memory_size() {
    std::cout << "Running test_custom_memory_size..." << std::endl;

    // This tests the Process class field, not the Scheduler logic (which is harder to unit test here)
    std::vector<Instruction> insts;
    auto p = std::make_shared<Process>(3, "test_mem", insts);

    p->set_memory_requirement(128);
    assert(p->get_memory_requirement() == 128);

    std::cout << "test_custom_memory_size PASSED" << std::endl;
}

int main() {
    try {
        test_memory_violation_read();
        test_memory_violation_write();
        test_custom_memory_size();

        std::cout << "\nAll Process Execution tests PASSED!" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Test failed: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
