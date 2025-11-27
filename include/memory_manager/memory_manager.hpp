#pragma once
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <iostream>
#include <mutex>
#include <optional>
#include <queue>
#include <random>
#include <set>
#include <thread>
#include <tuple>
#include <unordered_map>
#include <fstream>
#include <vector>
#include "memory_types.hpp"
#include "page_replacement_policy.hpp"


// Manages how pages are maintained and access for the scheduler. 
class MemoryManager {
public:
    MemoryManager(uint64_t total_mem,
                  uint64_t frame_size,
                  const std::string& backing_file_path);

    uint32_t allocate_process_memory(uint32_t pid, uint64_t bytes);
    bool free_process_memory(uint32_t pid);

    uint16_t read(uint32_t pid, uint64_t virtual_address, bool& fault);
    void write(uint32_t pid, uint64_t virtual_address, uint16_t value, bool& fault);

    
    void vmstat();
    void process_smi();
    void write_frame_logs();

private:
    uint64_t total_memory_;
    uint64_t frame_size_;
    uint32_t num_frames_;

    std::vector<Frame> frames_;
    std::unordered_map<uint32_t, std::vector<PageTableEntry>> page_tables_;
    std::unique_ptr<PageReplacementPolicy<Frame>> replacement_;
    std::fstream backing_store_;

    Frame* handle_page_fault(uint32_t pid, uint32_t page);
    Frame* find_free_frame();
    Frame* evict_frame();

    void load_page_from_backing(PageTableEntry& pte, Frame& frame);
    void write_page_to_backing(const PageTableEntry& pte, const Frame& frame);

    uint32_t virtual_to_page(uint64_t address) const { return address / frame_size_; }
    uint32_t offset_in_page(uint64_t address) const { return address % frame_size_; }
};
