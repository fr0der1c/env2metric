#include "otlp_payload_builder.hpp"

#include <cstdint>
#include <memory>
#include <string>

#include "cJSON.h"

#include "time_sync.hpp"

namespace {

struct JsonDeleter {
    void operator()(cJSON* value) const
    {
        cJSON_Delete(value);
    }
};

using JsonPtr = std::unique_ptr<cJSON, JsonDeleter>;

cJSON* add_object(cJSON* parent, const char* name)
{
    if (name == nullptr) {
        cJSON* item = cJSON_CreateObject();
        cJSON_AddItemToArray(parent, item);
        return item;
    }
    return cJSON_AddObjectToObject(parent, name);
}

cJSON* add_array(cJSON* parent, const char* name)
{
    return cJSON_AddArrayToObject(parent, name);
}

void add_string_attribute(cJSON* attributes, const char* key, const char* value)
{
    cJSON* attribute = add_object(attributes, nullptr);
    cJSON_AddStringToObject(attribute, "key", key);

    cJSON* attribute_value = add_object(attribute, "value");
    cJSON_AddStringToObject(attribute_value, "stringValue", value);
}

cJSON* add_metric(cJSON* metrics, const char* name, const char* description,
                  const char* unit, std::uint64_t time_nanos)
{
    cJSON* metric = add_object(metrics, nullptr);
    cJSON_AddStringToObject(metric, "name", name);
    cJSON_AddStringToObject(metric, "description", description);
    cJSON_AddStringToObject(metric, "unit", unit);

    cJSON* gauge = add_object(metric, "gauge");
    cJSON* points = add_array(gauge, "dataPoints");
    cJSON* point = add_object(points, nullptr);
    cJSON_AddStringToObject(point, "timeUnixNano",
                            std::to_string(time_nanos).c_str());
    return point;
}

void add_double_metric(cJSON* metrics, const char* name, const char* description,
                       const char* unit, double value,
                       std::uint64_t time_nanos)
{
    cJSON* point = add_metric(metrics, name, description, unit, time_nanos);
    cJSON_AddNumberToObject(point, "asDouble", value);
}

void add_int_metric(cJSON* metrics, const char* name, const char* description,
                    const char* unit, std::int64_t value,
                    std::uint64_t time_nanos)
{
    cJSON* point = add_metric(metrics, name, description, unit, time_nanos);
    cJSON_AddStringToObject(point, "asInt", std::to_string(value).c_str());
}

std::string print_json(cJSON* root)
{
    char* text = cJSON_PrintUnformatted(root);
    if (text == nullptr) {
        return {};
    }

    std::string result(text);
    cJSON_free(text);
    return result;
}

} // namespace

std::string build_air_quality_otlp_payload(const AirQualitySample& sample)
{
    const std::uint64_t time_nanos = current_unix_time_nanos();
    JsonPtr root(cJSON_CreateObject());
    if (root == nullptr) {
        return {};
    }

    cJSON* resource_metrics = add_array(root.get(), "resourceMetrics");
    cJSON* resource_metric = add_object(resource_metrics, nullptr);
    cJSON* resource = add_object(resource_metric, "resource");
    cJSON* attributes = add_array(resource, "attributes");
    add_string_attribute(attributes, "service.name", "env2metric");
    add_string_attribute(attributes, "device.name", "b39");
    add_string_attribute(attributes, "device.type", "air_quality_monitor");
    add_string_attribute(attributes, "runtime", "esp32s3");

    cJSON* scope_metrics = add_array(resource_metric, "scopeMetrics");
    cJSON* scope_metric = add_object(scope_metrics, nullptr);
    cJSON* scope = add_object(scope_metric, "scope");
    cJSON_AddStringToObject(scope, "name", "env2metric");
    cJSON* metrics = add_array(scope_metric, "metrics");

    add_double_metric(metrics, "env2metric.air.particle_0_3um",
                      ">0.3um particle count", "pcs/0.1L",
                      sample.particle_0_3um, time_nanos);
    add_double_metric(metrics, "env2metric.air.pm25", "PM2.5", "ug/m3",
                      sample.pm25, time_nanos);
    add_double_metric(metrics, "env2metric.air.hcho", "HCHO", "ug/m3",
                      sample.hcho, time_nanos);
    add_double_metric(metrics, "env2metric.air.co2", "CO2", "ppm", sample.co2,
                      time_nanos);
    add_double_metric(metrics, "env2metric.air.temperature", "Temperature",
                      "Cel", sample.temperature, time_nanos);
    add_double_metric(metrics, "env2metric.air.humidity", "Humidity", "%",
                      sample.humidity, time_nanos);
    if (sample.has_voc) {
        add_double_metric(metrics, "env2metric.air.voc", "VOC", "ppm",
                          voc_ppm(sample), time_nanos);
    }
    add_int_metric(metrics, "env2metric.air.sequence", "Device sequence number",
                   "1", sample.sequence, time_nanos);

    return print_json(root.get());
}
