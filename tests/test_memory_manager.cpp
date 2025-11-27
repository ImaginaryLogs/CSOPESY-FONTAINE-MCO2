#include "../include/paging/memory_manager.hpp"
#include "../include/config.hpp"
#include <iostream>
#include <cassert>
#include <filesystem>
#include <vector>
#include <cstring>

namespace fs = std::filesystem;

void test_initialization() {
    std::cout << "Running test_initialization..." << std::endl;
    Config cfg;
    cfg.max_overall_mem = 64; // 4 frames of 16 bytes
    cfg.mem_per_frame = 16;

    MemoryManager::getInstance().initialize(cfg);

    assert(MemoryManager::getInstance().get_total_frames() == 4);
    assert(MemoryManager::getInstance().get_free_frames_count() == 4);
    std::cout << "test_initialization PASSED" << std::endl;
}

void test_allocation() {
    std::cout << "Running test_allocation..." << std::endl;
    Config cfg;
    cfg.max_overall_mem = 64;
    cfg.mem_per_frame = 16;
    MemoryManager::getInstance().initialize(cfg);

    // Request page 0 for PID 1
    auto res1 = MemoryManager::getInstance().request_page(1, 0, false);
    assert(MemoryManager::getInstance().get_free_frames_count() == 3);
    assert(!res1.evicted_page.has_value());

    // Request page 1 for PID 1
    auto res2 = MemoryManager::getInstance().request_page(1, 1, false);
    assert(MemoryManager::getInstance().get_free_frames_count() == 2);

    std::cout << "test_allocation PASSED" << std::endl;
}

void test_fifo_eviction() {
    std::cout << "Running test_fifo_eviction..." << std::endl;
    Config cfg;
    cfg.max_overall_mem = 48; // 3 frames
    cfg.mem_per_frame = 16;
    MemoryManager::getInstance().initialize(cfg);

    // Fill memory (3 frames)
    MemoryManager::getInstance().request_page(1, 0, false); // Frame 0
    MemoryManager::getInstance().request_page(1, 1, false); // Frame 1
    MemoryManager::getInstance().request_page(1, 2, false); // Frame 2

    assert(MemoryManager::getInstance().get_free_frames_count() == 0);

    // Request 4th page, should evict Frame 0 (FIFO)
    auto res = MemoryManager::getInstance().request_page(1, 3, false);

    assert(res.evicted_page.has_value());
    assert(res.evicted_page->first == 1); // PID
    assert(res.evicted_page->second == 0); // Page 0 evicted

    std::cout << "test_fifo_eviction PASSED" << std::endl;
}

void test_backing_store() {
    std::cout << "Running test_backing_store..." << std::endl;

    // Clean up
    if (fs::exists("backing_store")) {
        fs::remove_all("backing_store");
    }

    Config cfg;
    cfg.max_overall_mem = 32; // 2 frames
    cfg.mem_per_frame = 16;
    MemoryManager::getInstance().initialize(cfg);

    // 1. Allocate page 0 for PID 1
    auto res1 = MemoryManager::getInstance().request_page(1, 0, false);
    size_t frame1 = res1.frame_idx;

    // 2. Write data to it
    MemoryManager::getInstance().write_physical(frame1, 0, 0xABCD);

    // 3. Allocate page 1 for PID 1 (fills memory)
    MemoryManager::getInstance().request_page(1, 1, false);

    // 4. Allocate page 2 for PID 1 (evicts page 0)
    auto res3 = MemoryManager::getInstance().request_page(1, 2, false);
    assert(res3.evicted_page.has_value());
    assert(res3.evicted_page->second == 0);

    // Verify swap file exists
    assert(fs::exists("backing_store/process_1.swap"));

    // 5. Request page 0 again (should load from disk)
    // This will evict page 1 (FIFO: 0->1->2, 0 evicted, now 1 is oldest)
    auto res4 = MemoryManager::getInstance().request_page(1, 0, true);
    size_t frame4 = res4.frame_idx;

    // 6. Verify data
    uint16_t val = MemoryManager::getInstance().read_physical(frame4, 0);
    assert(val == 0xABCD);

    std::cout << "test_backing_store PASSED" << std::endl;
}

void test_read_write_physical() {
    std::cout << "Running test_read_write_physical..." << std::endl;
    Config cfg;
    cfg.max_overall_mem = 64;
    cfg.mem_per_frame = 16;
    MemoryManager::getInstance().initialize(cfg);

    // Allocate a frame
    auto res = MemoryManager::getInstance().request_page(1, 0, false);
    size_t frame = res.frame_idx;

    // Test writing and reading uint16 values at different offsets
    // Offset 0
    MemoryManager::getInstance().write_physical(frame, 0, 0x1234);
    uint16_t val1 = MemoryManager::getInstance().read_physical(frame, 0);
    assert(val1 == 0x1234);

    // Offset 2 (next uint16 position)
    MemoryManager::getInstance().write_physical(frame, 2, 0xABCD);
    uint16_t val2 = MemoryManager::getInstance().read_physical(frame, 2);
    assert(val2 == 0xABCD);

    // Verify first value is still intact
    val1 = MemoryManager::getInstance().read_physical(frame, 0);
    assert(val1 == 0x1234);

    // Test boundary values
    MemoryManager::getInstance().write_physical(frame, 14, 0xFFFF); // Last uint16 in 16-byte frame
    uint16_t val3 = MemoryManager::getInstance().read_physical(frame, 14);
    assert(val3 == 0xFFFF);

    std::cout << "test_read_write_physical PASSED" << std::endl;
}

void test_multiple_processes() {
    std::cout << "Running test_multiple_processes..." << std::endl;
    Config cfg;
    cfg.max_overall_mem = 64; // 4 frames
    cfg.mem_per_frame = 16;
    MemoryManager::getInstance().initialize(cfg);

    // Process 1 allocates 2 pages
    auto p1_page0 = MemoryManager::getInstance().request_page(1, 0, false);
    auto p1_page1 = MemoryManager::getInstance().request_page(1, 1, false);

    // Process 2 allocates 2 pages
    auto p2_page0 = MemoryManager::getInstance().request_page(2, 0, false);
    auto p2_page1 = MemoryManager::getInstance().request_page(2, 1, false);

    // Memory should be full
    assert(MemoryManager::getInstance().get_free_frames_count() == 0);

    // Write unique data to each process
    MemoryManager::getInstance().write_physical(p1_page0.frame_idx, 0, 0x1111);
    MemoryManager::getInstance().write_physical(p1_page1.frame_idx, 0, 0x2222);
    MemoryManager::getInstance().write_physical(p2_page0.frame_idx, 0, 0x3333);
    MemoryManager::getInstance().write_physical(p2_page1.frame_idx, 0, 0x4444);

    // Verify data isolation
    assert(MemoryManager::getInstance().read_physical(p1_page0.frame_idx, 0) == 0x1111);
    assert(MemoryManager::getInstance().read_physical(p1_page1.frame_idx, 0) == 0x2222);
    assert(MemoryManager::getInstance().read_physical(p2_page0.frame_idx, 0) == 0x3333);
    assert(MemoryManager::getInstance().read_physical(p2_page1.frame_idx, 0) == 0x4444);

    std::cout << "test_multiple_processes PASSED" << std::endl;
}

void test_eviction_with_dirty_pages() {
    std::cout << "Running test_eviction_with_dirty_pages..." << std::endl;

    // Clean up
    if (fs::exists("backing_store")) {
        fs::remove_all("backing_store");
    }

    Config cfg;
    cfg.max_overall_mem = 32; // 2 frames
    cfg.mem_per_frame = 16;
    MemoryManager::getInstance().initialize(cfg);

    // Allocate and write to page 0
    auto res1 = MemoryManager::getInstance().request_page(1, 0, false);
    MemoryManager::getInstance().write_physical(res1.frame_idx, 0, 0x5555);

    // Allocate page 1 (fills memory)
    auto res2 = MemoryManager::getInstance().request_page(1, 1, false);
    MemoryManager::getInstance().write_physical(res2.frame_idx, 0, 0x6666);

    // Allocate page 2 (should evict page 0 which is dirty)
    auto res3 = MemoryManager::getInstance().request_page(1, 2, false);
    assert(res3.evicted_page.has_value());
    assert(res3.evicted_page->second == 0); // Page 0 evicted

    // Verify backing store file was created
    assert(fs::exists("backing_store/process_1.swap"));

    // Load page 0 back and verify data persisted
    auto res4 = MemoryManager::getInstance().request_page(1, 0, true);
    uint16_t val = MemoryManager::getInstance().read_physical(res4.frame_idx, 0);
    assert(val == 0x5555);

    std::cout << "test_eviction_with_dirty_pages PASSED" << std::endl;
}

void test_page_statistics() {
    std::cout << "Running test_page_statistics..." << std::endl;

    // Clean up
    if (fs::exists("backing_store")) {
        fs::remove_all("backing_store");
    }

    Config cfg;
    cfg.max_overall_mem = 32; // 2 frames
    cfg.mem_per_frame = 16;
    MemoryManager::getInstance().initialize(cfg);

    size_t initial_paged_in = MemoryManager::getInstance().get_paged_in_count();
    size_t initial_paged_out = MemoryManager::getInstance().get_paged_out_count();

    // Allocate pages to fill memory
    MemoryManager::getInstance().request_page(1, 0, false);
    MemoryManager::getInstance().request_page(1, 1, false);

    // No evictions yet
    assert(MemoryManager::getInstance().get_paged_out_count() == initial_paged_out);

    // Write to make them dirty
    auto frames = MemoryManager::getInstance().get_ram_state();
    for (const auto& [frame_idx, frame_info] : frames) {
        MemoryManager::getInstance().write_physical(frame_idx, 0, 0x1234);
    }

    // Force eviction by requesting another page
    MemoryManager::getInstance().request_page(1, 2, false);

    // Should have one page out
    assert(MemoryManager::getInstance().get_paged_out_count() == initial_paged_out + 1);

    // Request the evicted page back
    MemoryManager::getInstance().request_page(1, 0, true);

    // Should have one page in
    assert(MemoryManager::getInstance().get_paged_in_count() == initial_paged_in + 1);

    std::cout << "test_page_statistics PASSED" << std::endl;
}

int main() {
    try {
        test_initialization();
        test_allocation();
        test_fifo_eviction();
        test_backing_store();
        test_read_write_physical();
        test_multiple_processes();
        test_eviction_with_dirty_pages();
        test_page_statistics();

        std::cout << "\n========================================" << std::endl;
        std::cout << "All MemoryManager tests PASSED!" << std::endl;
        std::cout << "========================================\n" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Test failed: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
