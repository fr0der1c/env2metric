#pragma once

#include <cstdint>

bool sync_system_time();
bool has_synchronized_time();

std::uint64_t current_unix_time_nanos();
