#include "otlp_exporter.hpp"

#include <string>

#include "esp_http_client.h"
#include "esp_log.h"
#include "sdkconfig.h"

#include "otlp_payload_builder.hpp"
#include "time_sync.hpp"

namespace {

constexpr char kTag[] = "env2metric_otlp";

bool is_success_status(int status_code)
{
    return status_code >= 200 && status_code < 300;
}

} // namespace

bool export_air_quality_sample(const AirQualitySample& sample)
{
    if (!has_synchronized_time()) {
        ESP_LOGW(kTag, "refusing OTLP export before system time is synchronized");
        return false;
    }

    const std::string payload = build_air_quality_otlp_payload(sample);
    if (payload.empty()) {
        ESP_LOGE(kTag, "failed to build OTLP payload");
        return false;
    }

    esp_http_client_config_t config = {};
    config.url = CONFIG_ENV2METRIC_OTLP_METRICS_URL;
    config.timeout_ms = CONFIG_ENV2METRIC_OTLP_HTTP_TIMEOUT_MS;

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == nullptr) {
        ESP_LOGE(kTag, "failed to initialize OTLP HTTP client");
        return false;
    }

    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, payload.c_str(),
                                   static_cast<int>(payload.size()));

    const esp_err_t err = esp_http_client_perform(client);
    const int status_code = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);

    if (err != ESP_OK) {
        ESP_LOGW(kTag, "OTLP air quality export failed: %s",
                 esp_err_to_name(err));
        return false;
    }

    if (!is_success_status(status_code)) {
        ESP_LOGW(kTag, "OTLP air quality export returned HTTP %d",
                 status_code);
        return false;
    }

    ESP_LOGI(kTag, "OTLP air quality export succeeded, HTTP %d", status_code);
    return true;
}
