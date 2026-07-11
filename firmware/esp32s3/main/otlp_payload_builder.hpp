#pragma once

#include <string>

#include "air_quality_sample.hpp"

std::string build_air_quality_otlp_payload(const AirQualitySample& sample);
