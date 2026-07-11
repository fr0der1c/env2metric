#pragma once

#include <cstdint>

#include "esp_err.h"

struct RgbColor {
    std::uint8_t red = 0;
    std::uint8_t green = 0;
    std::uint8_t blue = 0;
};

esp_err_t init_status_led();
esp_err_t set_status_led(RgbColor color);
esp_err_t clear_status_led();
esp_err_t start_status_led_blink(RgbColor color, std::uint32_t on_ms,
                                 std::uint32_t off_ms);
esp_err_t stop_status_led_blink();
