#pragma once

#include <string>

#include "air_quality_sample.hpp"

struct ParseAirQualityResult {
    bool ok = false;
    AirQualitySample sample;
    std::string error;
};

ParseAirQualityResult parse_air_quality_line(const char* line);
