#pragma once

#include <cstdint>

struct AirQualitySample {
    double particle_0_3um = 0.0;
    double pm25 = 0.0;
    double hcho = 0.0;
    double co2 = 0.0;
    double temperature = 0.0;
    double humidity = 0.0;
    bool has_voc = false;
    double voc_raw_ppb = 0.0;
    std::int64_t sequence = 0;
};

inline double voc_ppm(const AirQualitySample& sample)
{
    return sample.has_voc ? sample.voc_raw_ppb / 1000.0 : 0.0;
}
