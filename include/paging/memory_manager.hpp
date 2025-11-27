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
#include <vector>

#include <vector>
#include <unordered_map>
#include <mutex>
#include <deque>
#include <string>
#include <fstream>
#include <optional>
#include <memory>
#include "../../include/config.hpp"

class MemoryManager {
public:
    static MemoryManager& getInstance();

    void initialize(const Config& cfg);

    // Returns byte at virtual address. Throws/Returns error if page fault.
    // We'll use a return type that can indicate page fault.
    // For simplicity, let's return an optional, or use a specific value/exception.
    // Since we can't easily throw exceptions that the scheduler handles gracefully without try-catch blocks everywhere,
    // let's have a method `check_access` or similar, or just return a status.
    // Actually, `read` and `write` will be called by Process.
    // If page fault, Process should know.

    // Physical Memory Access (called by Process after translation)
    uint16_t read_physical(size_t frame_idx, size_t offset);
    void write_physical(size_t frame_idx, size_t offset, uint16_t value);

    struct AllocationResult {
        size_t frame_idx;
        std::optional<std::pair<uint32_t, size_t>> evicted_page; // {pid, page_num}
    };

    // Allocates a frame for a page.
    // If load_from_disk is true, reads content from backing store.
    // If false, zeroes out the frame (new allocation).
    // Returns AllocationResult containing the new frame index and any evicted page info.
    AllocationResult request_page(uint32_t pid, size_t page_num, bool load_from_disk);

    // Debug/Visualization
    size_t get_free_frames_count() const;
    size_t get_total_frames() const;
    size_t get_paged_in_count() const;
    size_t get_paged_out_count() const;

    // For process-smi
    struct FrameInfo {
        uint32_t pid;
        size_t page_num;
        bool dirty; // This needs to be tracked by MemoryManager or Process?
                    // If Process tracks dirty bit in PageTable, MemoryManager might not know.
                    // But MemoryManager needs to know if it should write to disk on eviction.
                    // Let's say MemoryManager tracks dirty status of FRAMES.
    };
    // Returns map of FrameID -> FrameInfo
    std::unordered_map<size_t, FrameInfo> get_ram_state() const;

    // Mark a frame as dirty (called by Process on write)
    void mark_dirty(size_t frame_idx);

private:
    MemoryManager() = default;
    ~MemoryManager() = default;
    MemoryManager(const MemoryManager&) = delete;
    MemoryManager& operator=(const MemoryManager&) = delete;

    Config cfg_;
    std::vector<uint8_t> ram_;
    std::vector<bool> frame_map_; // true = used, false = free
    std::vector<bool> dirty_map_; // true = dirty

    // Reverse mapping: Frame -> (PID, PageNum)
    struct FrameOwner {
        uint32_t pid;
        size_t page_num;
    };
    std::vector<FrameOwner> frame_owners_;

    // FIFO Queue for page replacement
    std::deque<size_t> active_frames_;

    // Stats
    size_t paged_in_count_ = 0;
    size_t paged_out_count_ = 0;

    mutable std::mutex mtx_;

    // Backing Store Helpers
    std::string get_swap_filename(uint32_t pid);
    void save_frame_to_disk(uint32_t pid, size_t page_num, size_t frame_idx);
    void load_frame_from_disk(uint32_t pid, size_t page_num, size_t frame_idx);

    // Helper to find a free frame or evict
    // Returns {frame_idx, optional<evicted_info>}
    std::pair<size_t, std::optional<FrameOwner>> get_free_frame_or_evict();
};
