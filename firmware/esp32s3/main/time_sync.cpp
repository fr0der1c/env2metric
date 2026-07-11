#include "time_sync.hpp"

#include <cstdint>
#include <ctime>
#include <sys/time.h>

#include "esp_err.h"
#include "esp_log.h"
#include "esp_netif_sntp.h"
#include "freertos/FreeRTOS.h"
#include "sdkconfig.h"

namespace {

constexpr char kTag[] = "env2metric_time";
constexpr std::uint64_t kNanosPerSecond = 1000000000ULL;
constexpr std::uint64_t kNanosPerMicrosecond = 1000ULL;

bool sntp_started = false;

bool has_reasonable_time()
{
    std::time_t now = 0;
    std::time(&now);

    std::tm timeinfo = {};
    localtime_r(&now, &timeinfo);
    return timeinfo.tm_year >= (2024 - 1900);
}

void start_sntp_once()
{
    if (sntp_started) {
        return;
    }

    esp_sntp_config_t config =
        ESP_NETIF_SNTP_DEFAULT_CONFIG(CONFIG_ENV2METRIC_SNTP_SERVER);
    const esp_err_t err = esp_netif_sntp_init(&config);
    if (err == ESP_OK || err == ESP_ERR_INVALID_STATE) {
        sntp_started = true;
        return;
    }

    ESP_LOGW(kTag, "failed to initialize SNTP: %s", esp_err_to_name(err));
}

} // namespace

bool sync_system_time()
{
    if (has_reasonable_time()) {
        ESP_LOGI(kTag, "system time already synchronized");
        return true;
    }

    start_sntp_once();
    if (!sntp_started) {
        return false;
    }

    const esp_err_t err = esp_netif_sntp_sync_wait(
        pdMS_TO_TICKS(CONFIG_ENV2METRIC_SNTP_TIMEOUT_MS));
    if (err == ESP_OK && has_reasonable_time()) {
        ESP_LOGI(kTag, "system time synchronized");
        return true;
    }

    ESP_LOGW(kTag, "SNTP sync did not complete: %s", esp_err_to_name(err));
    return false;
}

bool has_synchronized_time()
{
    return has_reasonable_time();
}

std::uint64_t current_unix_time_nanos()
{
    timeval now = {};
    gettimeofday(&now, nullptr);
    return static_cast<std::uint64_t>(now.tv_sec) * kNanosPerSecond +
           static_cast<std::uint64_t>(now.tv_usec) * kNanosPerMicrosecond;
}
