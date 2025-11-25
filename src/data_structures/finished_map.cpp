#include "data_structures/finished_map.hpp"
#include <sstream>
#include <iomanip>
#include <ctime>
#include <bits/stdc++.h>

void FinishedMap::insert(ProcessPtr p, uint32_t finished_tick)
{
    std::scoped_lock lock(this->mutex_);
    if (!p)
        return;
    if (p->finished_logged)
        return; // needs a bool in Process
    p->finished_logged = true;

    time_t now = std::time(nullptr);

    finished_by_time_.emplace(now, std::make_shared<OrderedFinishedEntry>(finished_tick, p));
}

// Clear all finished processes
void FinishedMap::clear()
{
    std::scoped_lock<std::mutex> lock(mutex_);
    finished_by_time_.clear();
}

// Count
size_t FinishedMap::size()
{
    std::scoped_lock<std::mutex> lock(mutex_);
    return finished_by_time_.size();
}

std::string FinishedMap::snapshot()
{
    std::scoped_lock lock(this->mutex_);
    std::ostringstream oss;
    uint16_t counter = 0;
    uint16_t top = finished_by_time_.size();
    const int ui_showlimit = 10;
    if (finished_by_time_.empty())
        return "";

    oss << "Finished Time\tName\tProgress\t#\n";
    oss << "------------------------------------\n";

    for (auto it = finished_by_time_.rbegin(); it != finished_by_time_.rend(); ++it)
    {
        const auto t = it->first;
        const auto entryPtr = it->second;
        if (counter >= ui_showlimit)
            break; // <-- limit to top 10

        auto &[tick, p] = *entryPtr;

        std::tm tm_buf{};
        localtime_r(&t, &tm_buf);

        oss << std::put_time(&tm_buf, "%d-%m-%Y %H:%M:%S") << "\t"
            << p->name() << "\t"
            << p->get_executed_instructions() << " / "
            << p->get_total_instructions() << "\t"
            << top-- << "\n";
        ++counter;
    }

    if (finished_by_time_.size() > ui_showlimit)
        oss << "... (" << finished_by_time_.size() - ui_showlimit << " more)\n";

    return oss.str();
}

std::string FinishedMap::print()
{
    std::scoped_lock lock(this->mutex_);
    std::ostringstream oss;
    uint16_t counter = 0;
    uint16_t top = finished_by_time_.size();
    const int ui_showlimit = 10;
    if (finished_by_time_.empty())
        return "";

    oss << "Finished Time\tName\tProgress\t#\n";
    oss << "------------------------------------\n";

    for (auto it = finished_by_time_.rbegin(); it != finished_by_time_.rend(); ++it)
    {
        const auto t = it->first;
        const auto entryPtr = it->second;

        auto &[tick, p] = *entryPtr;

        std::tm tm_buf{};
        localtime_r(&t, &tm_buf);

        oss << std::put_time(&tm_buf, "%d-%m-%Y %H:%M:%S") << "\t"
            << p->name() << "\t"
            << p->get_executed_instructions() << " / "
            << p->get_total_instructions() << "\t"
            << top-- << "\n";
    }

    return oss.str();
}
