#pragma once

#include <cstdint>

class SensorRestartSchedule {
public:
    static constexpr std::int64_t kIntervalUs = 7LL * 24 * 60 * 60 * 1000000;

    explicit SensorRestartSchedule(std::int64_t connected_at_us)
        : next_attempt_us_(connected_at_us + kIntervalUs) {}

    // 每个周期最多领取一次复位机会；失败或错过多个周期也不连续补发。
    bool take_due_attempt(std::int64_t now_us)
    {
        if (now_us < next_attempt_us_) {
            return false;
        }
        next_attempt_us_ = now_us + kIntervalUs;
        return true;
    }

private:
    std::int64_t next_attempt_us_;
};
