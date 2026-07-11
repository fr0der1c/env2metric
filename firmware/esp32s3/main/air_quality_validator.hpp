#pragma once

#include <cstdint>
#include <string>

#include "air_quality_sample.hpp"

class AirQualityValidator {
public:
    bool validate(const AirQualitySample& sample, std::string& error);

private:
    bool has_last_sequence_ = false;
    std::int64_t last_sequence_ = 0;
};
