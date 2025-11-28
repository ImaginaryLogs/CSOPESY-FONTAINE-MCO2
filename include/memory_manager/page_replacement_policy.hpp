#pragma once
#include "memory_types.hpp"
#include <algorithm>
#include <deque>

template <typename FrameType>
class PageReplacementPolicy {
public:
    virtual FrameType* select_victim(std::vector<FrameType>& frames) = 0;
    virtual void on_access(FrameType& frame) = 0;
    virtual ~PageReplacementPolicy() = default;
};


template <typename FrameType>
class LRUReplacement : public PageReplacementPolicy<FrameType> {
    std::deque<uint32_t> usage_queue; // frame IDs; front = least recently used
public:
    FrameType* select_victim(std::vector<FrameType>& frames) override {
        for (auto id : usage_queue) {
            for (auto& f : frames) {
                if (f.id == id && !f.free)
                    return &f;
            }
        }
        return nullptr;
    }

    void on_access(FrameType& frame) override {
        usage_queue.erase(
            std::remove(usage_queue.begin(), usage_queue.end(), frame.id),
            usage_queue.end()
        );
        usage_queue.push_back(frame.id);
    }
};