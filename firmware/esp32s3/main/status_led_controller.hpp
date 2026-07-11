#pragma once

#include "esp_err.h"

esp_err_t init_status_led_controller();
esp_err_t show_wifi_waiting();
esp_err_t show_sensor_waiting();
esp_err_t show_fatal_error();
esp_err_t clear_status();
esp_err_t pulse_upload_started();
esp_err_t pulse_upload_success();
esp_err_t pulse_upload_failure();
esp_err_t pulse_invalid_sample();
