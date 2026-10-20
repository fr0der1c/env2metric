#include <cassert>
#include <cstdint>

#include "sensor_restart_schedule.hpp"

// 检查七天边界，确保一次连接不会在启动时或同一周期内反复复位。
void test_weekly_boundary()
{
    constexpr std::int64_t connected_at = 1234567;
    constexpr std::int64_t interval = 604800000000LL;
    static_assert(SensorRestartSchedule::kIntervalUs == interval);
    SensorRestartSchedule schedule(connected_at);
    assert(!schedule.take_due_attempt(connected_at));
    assert(!schedule.take_due_attempt(connected_at + interval - 1));
    assert(schedule.take_due_attempt(connected_at + interval));
    assert(!schedule.take_due_attempt(connected_at + interval));
    assert(!schedule.take_due_attempt(connected_at + 2 * interval - 1));
    assert(schedule.take_due_attempt(connected_at + 2 * interval));
}

// 模拟命令结果不明或长时间未检查，确保不会立即重试或补发多次复位。
void test_no_immediate_retry_or_catch_up()
{
    constexpr auto interval = SensorRestartSchedule::kIntervalUs;
    SensorRestartSchedule schedule(0);
    assert(schedule.take_due_attempt(3 * interval));
    assert(!schedule.take_due_attempt(3 * interval + 1000000));
    assert(!schedule.take_due_attempt(4 * interval - 1));
    assert(schedule.take_due_attempt(4 * interval));
}

// 模拟长期运行和重新连接，验证计时不依赖 32 位 tick，也不沿用断线前期限。
void test_long_uptime_and_reconnect()
{
    constexpr std::int64_t connected_at = (1LL << 32) * 1000;
    constexpr auto interval = SensorRestartSchedule::kIntervalUs;
    SensorRestartSchedule schedule(connected_at);
    assert(!schedule.take_due_attempt(connected_at + interval - 1));
    assert(schedule.take_due_attempt(connected_at + interval));

    SensorRestartSchedule reconnected(connected_at + interval + 1000000);
    assert(!reconnected.take_due_attempt(connected_at + 2 * interval));
    assert(reconnected.take_due_attempt(connected_at + 2 * interval + 1000000));
}

// 运行定期复位的边界、重复触发和重连计时回归测试。
int main()
{
    test_weekly_boundary();
    test_no_immediate_retry_or_catch_up();
    test_long_uptime_and_reconnect();
}
