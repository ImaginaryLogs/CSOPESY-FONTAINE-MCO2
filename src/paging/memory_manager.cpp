#include "../../include/paging/memory_manager.hpp"
#include <iostream>
#include <filesystem>
#include <cstring>
#include <algorithm>

namespace fs = std::filesystem;

MemoryManager& MemoryManager::getInstance() {
    static MemoryManager instance;
    return instance;
}

void MemoryManager::initialize(const Config& cfg) {
    std::lock_guard<std::mutex> lock(mtx_);
    cfg_ = cfg;

    // Resize RAM
    ram_.resize(cfg_.max_overall_mem, 0);

    // Calculate number of frames
    size_t num_frames = cfg_.max_overall_mem / cfg_.mem_per_frame;
    frame_map_.assign(num_frames, false); // All free
    dirty_map_.assign(num_frames, false); // All clean
    frame_owners_.assign(num_frames, {0, 0});

    active_frames_.clear();
    paged_in_count_ = 0;
    paged_out_count_ = 0;

    // Create backing store directory
    if (!fs::exists("backing_store")) {
        fs::create_directory("backing_store");
    }
}

uint16_t MemoryManager::read_physical(size_t frame_idx, size_t offset) {
    std::lock_guard<std::mutex> lock(mtx_);
    if (frame_idx >= frame_map_.size()) return 0; // Should not happen

    size_t addr = frame_idx * cfg_.mem_per_frame + offset;
    if (addr >= ram_.size()) return 0;

    // Read 2 bytes (uint16_t)
    // Assuming little-endian or consistent endianness
    // Check bounds for 2nd byte
    if (addr + 1 >= ram_.size()) return 0;

    uint16_t val = ram_[addr];
    val |= (static_cast<uint16_t>(ram_[addr + 1]) << 8);
    return val;
}

void MemoryManager::write_physical(size_t frame_idx, size_t offset, uint16_t value) {
    std::lock_guard<std::mutex> lock(mtx_);
    if (frame_idx >= frame_map_.size()) return;

    size_t addr = frame_idx * cfg_.mem_per_frame + offset;
    if (addr + 1 >= ram_.size()) return;

    ram_[addr] = static_cast<uint8_t>(value & 0xFF);
    ram_[addr + 1] = static_cast<uint8_t>((value >> 8) & 0xFF);

    dirty_map_[frame_idx] = true;
}

void MemoryManager::mark_dirty(size_t frame_idx) {
    std::lock_guard<std::mutex> lock(mtx_);
    if (frame_idx < dirty_map_.size()) {
        dirty_map_[frame_idx] = true;
    }
}

MemoryManager::AllocationResult MemoryManager::request_page(uint32_t pid, size_t page_num, bool load_from_disk) {
    std::lock_guard<std::mutex> lock(mtx_);

    auto [frame_idx, evicted] = get_free_frame_or_evict();

    // Bounds check to prevent segfault
    if (frame_idx >= frame_owners_.size()) {
        // This should never happen if initialized correctly
        std::cerr << "FATAL: frame_idx " << frame_idx << " out of bounds (size=" << frame_owners_.size() << ")\n";
        std::cerr << "  Requested by PID " << pid << ", page " << page_num << "\n";
        std::cerr << "  frame_map_.size()=" << frame_map_.size() << "\n";
        std::cerr << "  Is MemoryManager initialized? Check that initialize() was called.\n";
        // Return frame 0 as fallback to prevent crash, but system is in invalid state
        frame_idx = 0;
    }

    // Update frame owner
    frame_owners_[frame_idx] = {pid, page_num};
    frame_map_[frame_idx] = true;
    dirty_map_[frame_idx] = false; // Clean initially (unless we write to it)

    if (load_from_disk) {
        load_frame_from_disk(pid, page_num, frame_idx);
        paged_in_count_++;
    } else {
        // Zero out frame
        size_t start_addr = frame_idx * cfg_.mem_per_frame;
        std::fill(ram_.begin() + start_addr, ram_.begin() + start_addr + cfg_.mem_per_frame, 0);
    }

    // Add to active frames (FIFO)
    active_frames_.push_back(frame_idx);

    return {frame_idx, evicted ? std::optional<std::pair<uint32_t, size_t>>{{evicted->pid, evicted->page_num}} : std::nullopt};
}

std::pair<size_t, std::optional<MemoryManager::FrameOwner>> MemoryManager::get_free_frame_or_evict() {
    // Check for free frame
    for (size_t i = 0; i < frame_map_.size(); ++i) {
        if (!frame_map_[i]) {
            return {i, std::nullopt};
        }
    }

    // No free frame, evict victim (FIFO)
    if (active_frames_.empty()) {
        // Should not happen if size > 0
        return {0, std::nullopt};
    }

    size_t victim_frame = active_frames_.front();
    active_frames_.pop_front();

    FrameOwner victim = frame_owners_[victim_frame];

    // If dirty, write to disk
    if (dirty_map_[victim_frame]) {
        save_frame_to_disk(victim.pid, victim.page_num, victim_frame);
        paged_out_count_++;
    }

    return {victim_frame, victim};
}

std::string MemoryManager::get_swap_filename(uint32_t pid) {
    return "backing_store/process_" + std::to_string(pid) + ".swap";
}

void MemoryManager::save_frame_to_disk(uint32_t pid, size_t page_num, size_t frame_idx) {
    std::string filename = get_swap_filename(pid);
    std::fstream file(filename, std::ios::in | std::ios::out | std::ios::binary);
    if (!file) {
        // Create if not exists
        file.open(filename, std::ios::out | std::ios::binary);
        file.close();
        file.open(filename, std::ios::in | std::ios::out | std::ios::binary);
    }

    size_t offset = page_num * cfg_.mem_per_frame;
    file.seekp(offset);

    size_t ram_addr = frame_idx * cfg_.mem_per_frame;
    file.write(reinterpret_cast<char*>(&ram_[ram_addr]), cfg_.mem_per_frame);
}

void MemoryManager::load_frame_from_disk(uint32_t pid, size_t page_num, size_t frame_idx) {
    std::string filename = get_swap_filename(pid);
    std::ifstream file(filename, std::ios::binary);

    size_t ram_addr = frame_idx * cfg_.mem_per_frame;

    if (file) {
        size_t offset = page_num * cfg_.mem_per_frame;
        file.seekg(offset);
        file.read(reinterpret_cast<char*>(&ram_[ram_addr]), cfg_.mem_per_frame);

        if (file.gcount() < static_cast<std::streamsize>(cfg_.mem_per_frame)) {
            // Partial read or EOF, fill rest with 0
            size_t read_bytes = file.gcount();
            std::fill(ram_.begin() + ram_addr + read_bytes, ram_.begin() + ram_addr + cfg_.mem_per_frame, 0);
        }
    } else {
        // File doesn't exist (first access?), zero out
        std::fill(ram_.begin() + ram_addr, ram_.begin() + ram_addr + cfg_.mem_per_frame, 0);
    }
}

size_t MemoryManager::get_free_frames_count() const {
    std::lock_guard<std::mutex> lock(mtx_);
    size_t count = 0;
    for (bool used : frame_map_) {
        if (!used) count++;
    }
    return count;
}

size_t MemoryManager::get_total_frames() const {
    return frame_map_.size();
}

size_t MemoryManager::get_paged_in_count() const {
    return paged_in_count_;
}

size_t MemoryManager::get_paged_out_count() const {
    return paged_out_count_;
}

std::unordered_map<size_t, MemoryManager::FrameInfo> MemoryManager::get_ram_state() const {
    std::lock_guard<std::mutex> lock(mtx_);
    std::unordered_map<size_t, FrameInfo> state;
    for (size_t i = 0; i < frame_map_.size(); ++i) {
        if (frame_map_[i]) {
            state[i] = {frame_owners_[i].pid, frame_owners_[i].page_num, static_cast<bool>(dirty_map_[i])};
        }
    }
    return state;
}
