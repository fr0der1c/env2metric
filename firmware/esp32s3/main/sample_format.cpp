#include "sample_format.hpp"

#include <sstream>

std::string format_sample(const AirQualitySample& sample)
{
    std::ostringstream out;
    out << "particle_0_3um=" << sample.particle_0_3um
        << " pm25=" << sample.pm25
        << " hcho=" << sample.hcho
        << " co2=" << sample.co2
        << " temperature=" << sample.temperature
        << " humidity=" << sample.humidity;

    if (sample.has_voc) {
        out << " voc_ppm=" << voc_ppm(sample)
            << " voc_raw_ppb=" << sample.voc_raw_ppb;
    } else {
        out << " voc_ppm=<none>";
    }

    out << " sequence=" << sample.sequence;
    return out.str();
}
