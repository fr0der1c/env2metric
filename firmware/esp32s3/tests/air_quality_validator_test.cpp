#include <cassert>
#include <string>

#include "air_quality_validator.hpp"

// 模拟检测仪重连后的序号归零，验证连接内的序号和范围校验仍然有效。
int main()
{
    AirQualityValidator validator;
    AirQualitySample sample;
    sample.particle_0_3um = 100;
    sample.co2 = 400;
    sample.temperature = 25;
    sample.humidity = 50;
    sample.sequence = 1000;
    std::string error;

    assert(validator.validate(sample, error));
    assert(!validator.validate(sample, error));
    assert(error == "sequence did not increase");

    sample.sequence = 1;
    assert(!validator.validate(sample, error));
    validator.reset();
    assert(validator.validate(sample, error));
    assert(!validator.validate(sample, error));

    sample.sequence = 2;
    assert(validator.validate(sample, error));
    sample.sequence = 1;
    assert(!validator.validate(sample, error));

    validator.reset();
    sample.sequence = 0;
    assert(!validator.validate(sample, error));
    assert(error == "sequence must be positive");
    sample.sequence = 1;
    sample.co2 = 0;
    assert(!validator.validate(sample, error));
    assert(error == "co2 is lower than expected minimum 300 ppm");
    sample.co2 = 400;
    assert(validator.validate(sample, error));
}
