#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"

#include "status_led_controller.hpp"
#include "time_sync.hpp"
#include "usb_air_quality_reader.hpp"
#include "wifi_station.hpp"

namespace {

constexpr char kTag[] = "env2metric";
constexpr TickType_t kHoldDelay = pdMS_TO_TICKS(1000);
constexpr TickType_t kSntpRetryDelay = pdMS_TO_TICKS(5000);

void hold_fatal_error()
{
    ESP_ERROR_CHECK(show_fatal_error());
    while (true) {
        vTaskDelay(kHoldDelay);
    }
}

void wait_for_sntp_sync()
{
    while (!sync_system_time()) {
        ESP_LOGW(kTag, "waiting for SNTP before starting sensor collection");
        ESP_ERROR_CHECK(show_wifi_waiting());
        vTaskDelay(kSntpRetryDelay);
    }
}

} // namespace

extern "C" void app_main(void)
{
    ESP_LOGI(kTag, "env2metric ESP32-S3 firmware booting");
    ESP_LOGI(kTag, "status LED configured on GPIO48");
    ESP_ERROR_CHECK(init_status_led_controller());

    ESP_ERROR_CHECK(show_wifi_waiting());
    const WifiConnectResult wifi_result = connect_wifi_station();
    if (wifi_result == WifiConnectResult::connected) {
        wait_for_sntp_sync();
        ESP_ERROR_CHECK(clear_status());
        run_usb_air_quality_collector();
    } else if (wifi_result == WifiConnectResult::missing_config) {
        hold_fatal_error();
    } else {
        hold_fatal_error();
    }
}
