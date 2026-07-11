#include "air_quality_validator.hpp"

bool AirQualityValidator::validate(const AirQualitySample& sample,
                                   std::string& error)
{
    if (sample.particle_0_3um <= 0) {
        error = "particle_0_3um must be positive";
        return false;
    }
    if (sample.pm25 < 0 || sample.pm25 > 1000) {
        error = "pm25 is outside expected range [0, 1000]";
        return false;
    }
    if (sample.hcho < 0) {
        error = "hcho must not be negative";
        return false;
    }
    if (sample.co2 < 300) {
        error = "co2 is lower than expected minimum 300 ppm";
        return false;
    }
    if (sample.temperature < 10 || sample.temperature > 45) {
        error = "temperature is outside expected range [10, 45]";
        return false;
    }
    if (sample.humidity < 0 || sample.humidity > 99) {
        error = "humidity is outside expected range [0, 99]";
        return false;
    }
    if (sample.has_voc && sample.voc_raw_ppb < 0) {
        error = "voc must not be negative";
        return false;
    }
    if (sample.sequence <= 0) {
        error = "sequence must be positive";
        return false;
    }
    if (has_last_sequence_ && sample.sequence <= last_sequence_) {
        error = "sequence did not increase";
        return false;
    }

    has_last_sequence_ = true;
    last_sequence_ = sample.sequence;
    return true;
}
