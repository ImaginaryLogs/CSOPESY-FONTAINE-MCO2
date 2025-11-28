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

// Logical Virtual Memory is divided into pages, and it is stored into the page table. 
struct PageTableEntry {
  bool valid = false;
  bool dirty = false;
  uint32_t frame_number = 0;
  uint32_t backing_offset  = 0;
};

struct Frame {
  uint32_t id;
  bool free = true;
  uint32_t process_id = 0;
  uint32_t page_number = 0;
  std::vector<uint8_t> data;
};