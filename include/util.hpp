#pragma once
#include <cstdint>
#include <string>
#include <set>
#include <map>
#include <deque> 
#include <sstream>
#include <memory>
#include <functional>
#include <condition_variable>
#include "config.hpp"
#include "processes/process.hpp"
#include "util.hpp"
#include <cassert>
#include <queue>

uint16_t clamp_uint16(int64_t v);
std::string now_iso();

struct CpuUtilization {
    unsigned used;
    unsigned total;
    double percent;
    std::string to_string() const {
        std::ostringstream oss;
        oss << static_cast<int>(percent) << "%";
        return oss.str();
    }
};