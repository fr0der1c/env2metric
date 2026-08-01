#include "otlp_exporter.hpp"

#include <string>

#include "esp_crt_bundle.h"
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

/**
 * 按 OpenTelemetry 环境变量约定解析逗号分隔的 name=value 请求头，
 * 让固件可对接不同 OTLP 服务端的认证方式。
 */
void set_configured_headers(esp_http_client_handle_t client)
{
    const std::string headers = CONFIG_ENV2METRIC_OTLP_HEADERS;
    std::size_t start = 0;

    while (start < headers.size()) {
        const std::size_t end = headers.find(',', start);
        const std::string header = headers.substr(start, end - start);
        const std::size_t separator = header.find('=');
        const std::size_t name_start = header.find_first_not_of(" \t");
        const std::size_t value_start = separator == std::string::npos
                                            ? std::string::npos
                                            : header.find_first_not_of(" \t", separator + 1);

        if (separator == std::string::npos || name_start == std::string::npos ||
            name_start >= separator || value_start == std::string::npos) {
            ESP_LOGW(kTag, "ignoring invalid OTLP HTTP header entry");
        } else {
            const std::size_t name_end = header.find_last_not_of(" \t", separator - 1);
            const std::size_t value_end = header.find_last_not_of(" \t");
            const std::string name = header.substr(name_start, name_end - name_start + 1);
            const std::string value = header.substr(value_start, value_end - value_start + 1);
            esp_http_client_set_header(client, name.c_str(), value.c_str());
        }

        if (end == std::string::npos) {
            break;
        }
        start = end + 1;
    }
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
    config.crt_bundle_attach = esp_crt_bundle_attach;

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == nullptr) {
        ESP_LOGE(kTag, "failed to initialize OTLP HTTP client");
        return false;
    }

    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    set_configured_headers(client);
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
