#include "memory_manager/memory_manager.hpp"
#include "memory_manager/page_replacement_policy.hpp"

MemoryManager::MemoryManager(
        uint64_t total_mem,
        uint64_t frame_size,
        const std::string& backing_file_path)
    : total_memory_(total_mem),
      frame_size_(frame_size),
      num_frames_(total_mem / frame_size)
{
    frames_.reserve(num_frames_);
    for (uint32_t i = 0; i < num_frames_; i++) {
        frames_.push_back(Frame{
          .id=i, 
          .free=true, 
          .process_id=0, 
          .page_number=0, 
          .data=std::vector<uint8_t>(frame_size_)
        });
    }

    replacement_ = std::make_unique<LRUReplacement<Frame>>();

    backing_store_.open(backing_file_path, std::ios::in | std::ios::out | std::ios::binary);
    if (!backing_store_) {
        // If file doesn't exist, create it
        backing_store_.open(backing_file_path, std::ios::out | std::ios::binary);
        backing_store_.close();
        backing_store_.open(backing_file_path, std::ios::in | std::ios::out | std::ios::binary);
    }
}

uint32_t MemoryManager::allocate_process_memory(uint32_t pid, uint64_t bytes) {
    uint32_t pages = bytes / frame_size_;
    if (bytes % frame_size_) pages++;

    std::vector<PageTableEntry> table(pages);
    page_tables_[pid] = table;

    return pages;
}

bool MemoryManager::free_process_memory(uint32_t pid) {
    if (!page_tables_.contains(pid)) return false;

    // Free frames
    for (auto& f : frames_) {
        if (f.process_id == pid) {
            f.free = true;
            f.process_id = 0;
        }
    }

    page_tables_.erase(pid);
    return true;
}

uint16_t MemoryManager::read(uint32_t pid, uint64_t address, bool& fault) {
    uint32_t page = virtual_to_page(address);
    auto& pte = page_tables_[pid][page];

    Frame* frame = nullptr;

    if (!pte.valid) {
        fault = true;
        frame = handle_page_fault(pid, page);
    } else {
        frame = &frames_[pte.frame_number];
    }

    replacement_->on_access(*frame);
    return *reinterpret_cast<uint16_t*>(&frame->data[offset_in_page(address)]);
}

void MemoryManager::write(uint32_t pid, uint64_t address, uint16_t value, bool& fault) {
    uint32_t page = virtual_to_page(address);
    auto& pte = page_tables_[pid][page];

    Frame* frame = nullptr;

    if (!pte.valid) {
        fault = true;
        frame = handle_page_fault(pid, page);
    } else {
        frame = &frames_[pte.frame_number];
    }

    replacement_->on_access(*frame);
    pte.dirty = true;

    *reinterpret_cast<uint16_t*>(&frame->data[offset_in_page(address)]) = value;
}

/**************** Page Fault Handling ****************/

Frame* MemoryManager::handle_page_fault(uint32_t pid, uint32_t page) {
    Frame* frame = find_free_frame();
    auto& pte = page_tables_[pid][page];

    if (!frame) {
        frame = evict_frame();
    }

    frame->free = false;
    frame->process_id = pid;
    frame->page_number = page;

    load_page_from_backing(pte, *frame);

    pte.valid = true;
    pte.frame_number = frame->id;

    return frame;
}

// Linear search for a free frame
Frame* MemoryManager::find_free_frame() {
    for (auto& frame : frames_) {
        if (frame.free)
            return &frame;
    }
    return nullptr;
}

// Among 
Frame* MemoryManager::evict_frame() {
    Frame* victim = replacement_->select_victim(frames_);
    auto& pte = page_tables_[victim->process_id][victim->page_number];

    if (pte.dirty) 
        write_page_to_backing(pte, *victim);

    pte.valid = false;
    victim->free = true;
    return victim;
}

void MemoryManager::load_page_from_backing(PageTableEntry& pte, Frame& frame) {
    backing_store_.seekg(pte.backing_offset);
    backing_store_.read(reinterpret_cast<char*>(frame.data.data()), frame_size_);
}

void MemoryManager::write_page_to_backing(const PageTableEntry& pte, const Frame& frame) {
    backing_store_.seekp(pte.backing_offset);
    backing_store_.write(reinterpret_cast<const char*>(frame.data.data()), frame_size_);
}
