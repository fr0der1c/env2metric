#include "status_led.hpp"

#include "driver/gpio.h"
#include "esp_check.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "led_strip.h"

namespace {

constexpr char kTag[] = "env2metric_led";
constexpr gpio_num_t kStatusLedGpio = GPIO_NUM_48;
constexpr std::uint32_t kLedResolutionHz = 10 * 1000 * 1000;
constexpr UBaseType_t kBlinkTaskPriority = 3;
constexpr std::uint32_t kBlinkTaskStackSize = 2048;

led_strip_handle_t status_led = nullptr;
TaskHandle_t blink_task = nullptr;

struct BlinkConfig {
    RgbColor color;
    TickType_t on_delay = 0;
    TickType_t off_delay = 0;
};

BlinkConfig blink_config;

void blink_status_led_task(void *)
{
    while (true) {
        ESP_ERROR_CHECK(set_status_led(blink_config.color));
        vTaskDelay(blink_config.on_delay);
        ESP_ERROR_CHECK(clear_status_led());
        vTaskDelay(blink_config.off_delay);
    }
}

} // namespace

esp_err_t stop_status_led_blink();

esp_err_t init_status_led()
{
    led_strip_config_t strip_config = {};
    strip_config.strip_gpio_num = kStatusLedGpio;
    strip_config.max_leds = 1;

    led_strip_rmt_config_t rmt_config = {};
    rmt_config.resolution_hz = kLedResolutionHz;
    rmt_config.flags.with_dma = false;

    ESP_RETURN_ON_ERROR(
        led_strip_new_rmt_device(&strip_config, &rmt_config, &status_led), kTag,
        "failed to initialize status LED on GPIO48");
    return clear_status_led();
}

esp_err_t set_status_led(RgbColor color)
{
    ESP_RETURN_ON_ERROR(
        led_strip_set_pixel(status_led, 0, color.red, color.green, color.blue),
        kTag, "failed to set status LED pixel");
    ESP_RETURN_ON_ERROR(led_strip_refresh(status_led), kTag,
                        "failed to refresh status LED");
    return ESP_OK;
}

esp_err_t clear_status_led()
{
    ESP_RETURN_ON_ERROR(led_strip_clear(status_led), kTag,
                        "failed to clear status LED");
    return ESP_OK;
}

esp_err_t start_status_led_blink(RgbColor color, std::uint32_t on_ms,
                                 std::uint32_t off_ms)
{
    ESP_RETURN_ON_ERROR(stop_status_led_blink(), kTag,
                        "failed to stop previous LED blink task");

    blink_config.color = color;
    blink_config.on_delay = pdMS_TO_TICKS(on_ms);
    blink_config.off_delay = pdMS_TO_TICKS(off_ms);

    const BaseType_t created =
        xTaskCreate(blink_status_led_task, "status_led_blink",
                    kBlinkTaskStackSize, nullptr, kBlinkTaskPriority,
                    &blink_task);
    ESP_RETURN_ON_FALSE(created == pdPASS, ESP_ERR_NO_MEM, kTag,
                        "failed to create LED blink task");
    return ESP_OK;
}

esp_err_t stop_status_led_blink()
{
    if (blink_task != nullptr) {
        vTaskDelete(blink_task);
        blink_task = nullptr;
    }

    return clear_status_led();
}
