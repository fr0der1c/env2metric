#include "status_led_controller.hpp"

#include <cstdint>

#include "esp_check.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "status_led.hpp"

namespace {

constexpr char kTag[] = "env2metric_led_ctl";
constexpr std::uint32_t kFastBlinkDelayMs = 200;
constexpr TickType_t kPulseDelay = pdMS_TO_TICKS(500);

constexpr RgbColor kWifiWaiting{0, 0, 16};
constexpr RgbColor kSensorWaiting{16, 16, 0};
constexpr RgbColor kFatalError{16, 0, 0};
constexpr RgbColor kUploadSuccess{16, 16, 16};
constexpr RgbColor kUploadFailure{16, 8, 0};
constexpr RgbColor kInvalidSample{16, 16, 0};

enum class PersistentStatus {
    cleared,
    wifi_waiting,
    sensor_waiting,
    fatal_error,
};

SemaphoreHandle_t led_mutex = nullptr;
PersistentStatus persistent_status = PersistentStatus::cleared;

esp_err_t lock_controller()
{
    ESP_RETURN_ON_FALSE(led_mutex != nullptr, ESP_ERR_INVALID_STATE, kTag,
                        "LED controller is not initialized");
    ESP_RETURN_ON_FALSE(xSemaphoreTake(led_mutex, portMAX_DELAY) == pdTRUE,
                        ESP_ERR_TIMEOUT, kTag,
                        "failed to lock LED controller");
    return ESP_OK;
}

void unlock_controller()
{
    xSemaphoreGive(led_mutex);
}

esp_err_t apply_persistent_status()
{
    switch (persistent_status) {
    case PersistentStatus::cleared:
        return stop_status_led_blink();
    case PersistentStatus::wifi_waiting:
        return start_status_led_blink(kWifiWaiting, kFastBlinkDelayMs,
                                      kFastBlinkDelayMs);
    case PersistentStatus::sensor_waiting:
        return start_status_led_blink(kSensorWaiting, kFastBlinkDelayMs,
                                      kFastBlinkDelayMs);
    case PersistentStatus::fatal_error:
        ESP_RETURN_ON_ERROR(stop_status_led_blink(), kTag,
                            "failed to stop LED blink");
        return set_status_led(kFatalError);
    }

    return ESP_ERR_INVALID_STATE;
}

esp_err_t set_persistent_status(PersistentStatus status)
{
    ESP_RETURN_ON_ERROR(lock_controller(), kTag,
                        "failed to lock LED controller");
    persistent_status = status;
    const esp_err_t err = apply_persistent_status();
    unlock_controller();
    return err;
}

esp_err_t pulse(RgbColor color)
{
    ESP_RETURN_ON_ERROR(lock_controller(), kTag,
                        "failed to lock LED controller");
    esp_err_t err = stop_status_led_blink();
    if (err == ESP_OK) {
        err = set_status_led(color);
    }
    unlock_controller();
    ESP_RETURN_ON_ERROR(err, kTag, "failed to show LED pulse");

    vTaskDelay(kPulseDelay);

    ESP_RETURN_ON_ERROR(lock_controller(), kTag,
                        "failed to lock LED controller");
    err = apply_persistent_status();
    unlock_controller();
    return err;
}

} // namespace

esp_err_t init_status_led_controller()
{
    led_mutex = xSemaphoreCreateMutex();
    ESP_RETURN_ON_FALSE(led_mutex != nullptr, ESP_ERR_NO_MEM, kTag,
                        "failed to create LED controller mutex");
    persistent_status = PersistentStatus::cleared;
    return init_status_led();
}

esp_err_t show_wifi_waiting()
{
    return set_persistent_status(PersistentStatus::wifi_waiting);
}

esp_err_t show_sensor_waiting()
{
    return set_persistent_status(PersistentStatus::sensor_waiting);
}

esp_err_t show_fatal_error()
{
    return set_persistent_status(PersistentStatus::fatal_error);
}

esp_err_t clear_status()
{
    return set_persistent_status(PersistentStatus::cleared);
}

esp_err_t pulse_upload_started()
{
    return pulse(kUploadSuccess);
}

esp_err_t pulse_upload_success()
{
    return pulse(kUploadSuccess);
}

esp_err_t pulse_upload_failure()
{
    return pulse(kUploadFailure);
}

esp_err_t pulse_invalid_sample()
{
    return pulse(kInvalidSample);
}
