#include "wifi_station.hpp"

#include <cstdint>
#include <cstring>

#include "esp_check.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "nvs_flash.h"
#include "sdkconfig.h"

#include "status_led_controller.hpp"

namespace {

constexpr char kTag[] = "env2metric_wifi";
constexpr EventBits_t kConnectedBit = BIT0;

EventGroupHandle_t wifi_event_group = nullptr;

bool has_wifi_ssid()
{
    return std::strlen(CONFIG_ENV2METRIC_WIFI_SSID) > 0;
}

wifi_auth_mode_t minimum_auth_mode()
{
    if (std::strlen(CONFIG_ENV2METRIC_WIFI_PASSWORD) == 0) {
        return WIFI_AUTH_OPEN;
    }

    return WIFI_AUTH_WPA2_PSK;
}

esp_err_t init_nvs()
{
    esp_err_t err = nvs_flash_init();
    if (err != ESP_ERR_NVS_NO_FREE_PAGES &&
        err != ESP_ERR_NVS_NEW_VERSION_FOUND) {
        return err;
    }

    ESP_RETURN_ON_ERROR(nvs_flash_erase(), kTag, "failed to erase NVS");
    return nvs_flash_init();
}

void handle_wifi_event(void *, esp_event_base_t event_base,
                       std::int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        ESP_LOGI(kTag, "Wi-Fi station started");
        ESP_ERROR_CHECK(esp_wifi_connect());
        return;
    }

    if (event_base != WIFI_EVENT || event_id != WIFI_EVENT_STA_DISCONNECTED) {
        return;
    }

    const auto *event = static_cast<wifi_event_sta_disconnected_t *>(event_data);
    ESP_LOGW(kTag, "Wi-Fi disconnected, reason=%d", event->reason);
    if (wifi_event_group != nullptr) {
        xEventGroupClearBits(wifi_event_group, kConnectedBit);
    }
    const esp_err_t led_err = show_wifi_waiting();
    if (led_err != ESP_OK) {
        ESP_LOGW(kTag, "failed to show Wi-Fi waiting LED: %s",
                 esp_err_to_name(led_err));
    }
    ESP_LOGI(kTag, "retrying Wi-Fi connection");
    ESP_ERROR_CHECK(esp_wifi_connect());
}

void handle_ip_event(void *, esp_event_base_t event_base, std::int32_t event_id,
                     void *event_data)
{
    if (event_base != IP_EVENT || event_id != IP_EVENT_STA_GOT_IP) {
        return;
    }

    const auto *event = static_cast<ip_event_got_ip_t *>(event_data);
    ESP_LOGI(kTag, "Wi-Fi connected, ip=" IPSTR, IP2STR(&event->ip_info.ip));
    xEventGroupSetBits(wifi_event_group, kConnectedBit);
    const esp_err_t led_err = clear_status();
    if (led_err != ESP_OK) {
        ESP_LOGW(kTag, "failed to clear Wi-Fi waiting LED: %s",
                 esp_err_to_name(led_err));
    }
}

esp_err_t configure_wifi()
{
    ESP_RETURN_ON_ERROR(init_nvs(), kTag, "failed to initialize NVS");
    ESP_RETURN_ON_ERROR(esp_netif_init(), kTag, "failed to initialize netif");
    ESP_RETURN_ON_ERROR(esp_event_loop_create_default(), kTag,
                        "failed to create default event loop");

    esp_netif_create_default_wifi_sta();

    wifi_init_config_t init_config = WIFI_INIT_CONFIG_DEFAULT();
    ESP_RETURN_ON_ERROR(esp_wifi_init(&init_config), kTag,
                        "failed to initialize Wi-Fi");

    ESP_RETURN_ON_ERROR(
        esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                            &handle_wifi_event, nullptr,
                                            nullptr),
        kTag, "failed to register Wi-Fi event handler");
    ESP_RETURN_ON_ERROR(
        esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                            &handle_ip_event, nullptr, nullptr),
        kTag, "failed to register IP event handler");

    wifi_config_t wifi_config = {};
    std::strncpy(reinterpret_cast<char *>(wifi_config.sta.ssid),
                 CONFIG_ENV2METRIC_WIFI_SSID,
                 sizeof(wifi_config.sta.ssid) - 1);
    std::strncpy(reinterpret_cast<char *>(wifi_config.sta.password),
                 CONFIG_ENV2METRIC_WIFI_PASSWORD,
                 sizeof(wifi_config.sta.password) - 1);
    wifi_config.sta.threshold.authmode = minimum_auth_mode();
    wifi_config.sta.sae_pwe_h2e = WPA3_SAE_PWE_BOTH;

    ESP_RETURN_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_STA), kTag,
                        "failed to set Wi-Fi station mode");
    ESP_RETURN_ON_ERROR(esp_wifi_set_config(WIFI_IF_STA, &wifi_config), kTag,
                        "failed to set Wi-Fi station config");
    return esp_wifi_start();
}

} // namespace

WifiConnectResult connect_wifi_station()
{
    if (!has_wifi_ssid()) {
        ESP_LOGW(kTag, "Wi-Fi SSID is not configured");
        return WifiConnectResult::missing_config;
    }

    wifi_event_group = xEventGroupCreate();
    if (wifi_event_group == nullptr) {
        ESP_LOGE(kTag, "failed to create Wi-Fi event group");
        return WifiConnectResult::failed;
    }

    const esp_err_t err = configure_wifi();
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "failed to configure Wi-Fi: %s", esp_err_to_name(err));
        return WifiConnectResult::failed;
    }

    xEventGroupWaitBits(wifi_event_group, kConnectedBit, pdFALSE, pdFALSE,
                        portMAX_DELAY);
    return WifiConnectResult::connected;
}

bool is_wifi_station_connected()
{
    if (wifi_event_group == nullptr) {
        return false;
    }

    return (xEventGroupGetBits(wifi_event_group) & kConnectedBit) != 0;
}
