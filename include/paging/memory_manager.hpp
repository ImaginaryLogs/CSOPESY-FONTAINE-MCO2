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

template <size_t PageSize, size_t PhysPages>
class MemoryManager {
  using PFN = size_t;
  using VFN = size_t;

  private:
    std::mutex memoryMtx_;
    std::vector<PFN> free_frames_;

    std::mutex tlb_mtx_;
    std::unordered_map<VPN, PFN> tlb_;
    size_t tlb_capacity_ = 64;
};