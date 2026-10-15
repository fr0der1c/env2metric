#include "usb_air_quality_reader.hpp"

#include <cassert>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>

#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "sdkconfig.h"
#include "usb/cdc_acm_host.h"
#include "usb/usb_host.h"
#include "usb/vcp.hpp"

#include "air_quality_parser.hpp"
#include "air_quality_validator.hpp"
#include "otlp_exporter.hpp"
#include "sample_format.hpp"
#include "status_led_controller.hpp"
#include "time_sync.hpp"
#include "wifi_station.hpp"

using namespace esp_usb;

namespace {

constexpr char kTag[] = "env2metric_usb";
constexpr std::uint16_t kMonitorVid = 0x303a;
constexpr std::uint16_t kMonitorPid = 0x1001;
constexpr std::uint8_t kMonitorInterface = 0;
constexpr std::size_t kMaxLineLength = 192;
constexpr UBaseType_t kQueueLength = 8;
constexpr TickType_t kOpenRetryDelay = pdMS_TO_TICKS(2000);
constexpr TickType_t kLinePollDelay = pdMS_TO_TICKS(1000);

struct RawSensorLine {
    char text[kMaxLineLength] = {};
};

SemaphoreHandle_t device_disconnected_sem = nullptr;

void update_status(esp_err_t err, const char* action)
{
    if (err != ESP_OK) {
        ESP_LOGW(kTag, "failed to %s: %s", action, esp_err_to_name(err));
    }
}

bool network_is_ready()
{
    if (!is_wifi_station_connected()) {
        ESP_LOGW(kTag, "Wi-Fi is not connected, skipping sample export");
        update_status(show_wifi_waiting(), "show Wi-Fi waiting status");
        return false;
    }

    if (!has_synchronized_time()) {
        ESP_LOGW(kTag, "system time is not synchronized, skipping sample export");
        update_status(show_wifi_waiting(), "show Wi-Fi waiting status");
        return false;
    }

    return true;
}

class LineAssembler {
public:
    explicit LineAssembler(QueueHandle_t line_queue) : line_queue_(line_queue) {}

    void reset()
    {
        length_ = 0;
    }

    void append(const std::uint8_t* data, std::size_t size)
    {
        for (std::size_t index = 0; index < size; ++index) {
            append_byte(static_cast<char>(data[index]));
        }
    }

private:
    void append_byte(char byte)
    {
        if (byte == '\r') {
            return;
        }
        if (byte == '\n') {
            flush_line();
            return;
        }
        if (length_ + 1 >= kMaxLineLength) {
            ESP_LOGW(kTag, "sensor line exceeded %u bytes, dropping partial line",
                     static_cast<unsigned>(kMaxLineLength));
            length_ = 0;
            return;
        }

        buffer_[length_++] = byte;
    }

    void flush_line()
    {
        if (length_ == 0) {
            return;
        }

        RawSensorLine line;
        std::memcpy(line.text, buffer_, length_);
        line.text[length_] = '\0';
        length_ = 0;

        if (xQueueSend(line_queue_, &line, 0) != pdTRUE) {
            ESP_LOGW(kTag, "sensor line queue is full, dropping line: %s",
                     line.text);
        }
    }

    QueueHandle_t line_queue_ = nullptr;
    char buffer_[kMaxLineLength] = {};
    std::size_t length_ = 0;
};

bool handle_rx(const std::uint8_t* data, std::size_t data_len, void* arg)
{
    auto* assembler = static_cast<LineAssembler*>(arg);
    assembler->append(data, data_len);
    return true;
}

void handle_event(const cdc_acm_host_dev_event_data_t* event, void* user_ctx)
{
    switch (event->type) {
    case CDC_ACM_HOST_ERROR:
        ESP_LOGE(kTag, "CDC-ACM error occurred, err_no=%d",
                 event->data.error);
        break;
    case CDC_ACM_HOST_DEVICE_DISCONNECTED:
        ESP_LOGW(kTag, "air quality monitor disconnected");
        xSemaphoreGive(device_disconnected_sem);
        break;
    case CDC_ACM_HOST_SERIAL_STATE:
        ESP_LOGI(kTag, "CDC serial state notification: 0x%04X",
                 event->data.serial_state.val);
        break;
    case CDC_ACM_HOST_NETWORK_CONNECTION:
        break;
    default:
        ESP_LOGW(kTag, "unsupported CDC event: %d", event->type);
        break;
    }
}

void handle_new_usb_device(usb_device_handle_t usb_device)
{
    const usb_device_desc_t* descriptor = nullptr;
    const esp_err_t err = usb_host_get_device_descriptor(usb_device, &descriptor);
    if (err != ESP_OK) {
        ESP_LOGW(kTag, "failed to read USB device descriptor: %s",
                 esp_err_to_name(err));
        return;
    }

    ESP_LOGI(kTag,
             "USB device detected: vid=0x%04x pid=0x%04x class=0x%02x subclass=0x%02x protocol=0x%02x",
             descriptor->idVendor, descriptor->idProduct,
             descriptor->bDeviceClass, descriptor->bDeviceSubClass,
             descriptor->bDeviceProtocol);
}

void usb_lib_task(void* arg)
{
    while (true) {
        std::uint32_t event_flags = 0;
        usb_host_lib_handle_events(portMAX_DELAY, &event_flags);
        if ((event_flags & USB_HOST_LIB_EVENT_FLAGS_NO_CLIENTS) != 0) {
            ESP_ERROR_CHECK(usb_host_device_free_all());
        }
        if ((event_flags & USB_HOST_LIB_EVENT_FLAGS_ALL_FREE) != 0) {
            ESP_LOGI(kTag, "USB host released all devices");
        }
    }
}

void configure_line_coding(CdcAcmDevice& device)
{
    cdc_acm_line_coding_t line_coding = {};
    line_coding.dwDTERate = CONFIG_ENV2METRIC_SENSOR_BAUD_RATE;
    line_coding.bCharFormat = 0;
    line_coding.bParityType = 0;
    line_coding.bDataBits = 8;

    esp_err_t err = device.line_coding_set(&line_coding);
    if (err != ESP_OK) {
        ESP_LOGW(kTag, "failed to set line coding, continuing: %s",
                 esp_err_to_name(err));
    }

    err = device.set_control_line_state(true, true);
    if (err != ESP_OK) {
        ESP_LOGW(kTag, "failed to set DTR/RTS, continuing: %s",
                 esp_err_to_name(err));
    }
}

std::unique_ptr<CdcAcmDevice> open_air_quality_device(
    const cdc_acm_host_device_config_t& device_config)
{
    auto device = std::make_unique<CdcAcmDevice>();
    const esp_err_t err =
        device->open(kMonitorVid, kMonitorPid, kMonitorInterface,
                     &device_config);
    if (err == ESP_OK) {
        ESP_LOGI(kTag,
                 "opened air quality monitor CDC-ACM device vid=0x%04x pid=0x%04x interface=%u",
                 kMonitorVid, kMonitorPid, kMonitorInterface);
        return device;
    }

    ESP_LOGW(kTag, "air quality monitor CDC-ACM open failed: %s",
             esp_err_to_name(err));
    return nullptr;
}

void process_sensor_line(const RawSensorLine& line,
                         AirQualityValidator& validator)
{
    ESP_LOGI(kTag, "sensor line: %s", line.text);

    ParseAirQualityResult parsed = parse_air_quality_line(line.text);
    if (!parsed.ok) {
        ESP_LOGW(kTag, "invalid sensor line: %s; line=%s",
                 parsed.error.c_str(), line.text);
        update_status(pulse_invalid_sample(), "pulse invalid sample status");
        return;
    }

    const std::string sample_text = format_sample(parsed.sample);
    std::string validation_error;
    if (!validator.validate(parsed.sample, validation_error)) {
        ESP_LOGW(kTag, "invalid sample: %s; sample={%s}",
                 validation_error.c_str(), sample_text.c_str());
        update_status(pulse_invalid_sample(), "pulse invalid sample status");
        return;
    }

    ESP_LOGI(kTag, "parsed sample: %s", sample_text.c_str());
    if (!network_is_ready()) {
        return;
    }

    update_status(pulse_upload_started(), "pulse upload started status");
    if (export_air_quality_sample(parsed.sample)) {
        update_status(pulse_upload_success(), "pulse upload success status");
        return;
    }

    if (!is_wifi_station_connected()) {
        update_status(show_wifi_waiting(), "show Wi-Fi waiting status");
        return;
    }

    update_status(pulse_upload_failure(), "pulse upload failure status");
}

void drain_stale_lines(QueueHandle_t queue)
{
    RawSensorLine line;
    while (xQueueReceive(queue, &line, 0) == pdTRUE) {
    }
}

} // namespace

// 持续采集检测仪数据，USB 断开后清空连接状态并重新打开设备。
void run_usb_air_quality_collector()
{
    QueueHandle_t line_queue =
        xQueueCreate(kQueueLength, sizeof(RawSensorLine));
    assert(line_queue != nullptr);

    device_disconnected_sem = xSemaphoreCreateBinary();
    assert(device_disconnected_sem != nullptr);

    ESP_LOGI(kTag, "installing USB Host");
    usb_host_config_t host_config = {};
    host_config.skip_phy_setup = false;
    host_config.intr_flags = ESP_INTR_FLAG_LEVEL1;
    ESP_ERROR_CHECK(usb_host_install(&host_config));

    BaseType_t task_created =
        xTaskCreate(usb_lib_task, "usb_lib", 4096, nullptr, 20, nullptr);
    assert(task_created == pdTRUE);

    ESP_LOGI(kTag, "installing CDC-ACM host driver");
    ESP_ERROR_CHECK(cdc_acm_host_install(nullptr));
    ESP_ERROR_CHECK(cdc_acm_host_register_new_dev_callback(
        handle_new_usb_device));

    LineAssembler assembler(line_queue);
    AirQualityValidator validator;

    while (true) {
        update_status(show_sensor_waiting(), "show sensor waiting status");
        drain_stale_lines(line_queue);

        cdc_acm_host_device_config_t device_config = {};
        device_config.connection_timeout_ms = 5000;
        device_config.out_buffer_size = 512;
        device_config.in_buffer_size = 512;
        device_config.event_cb = handle_event;
        device_config.data_cb = handle_rx;
        device_config.user_arg = &assembler;

        ESP_LOGI(kTag, "opening USB CDC-ACM air quality monitor");
        auto device = open_air_quality_device(device_config);
        if (device == nullptr) {
            ESP_LOGW(kTag, "no USB CDC-ACM device opened, retrying");
            vTaskDelay(kOpenRetryDelay);
            continue;
        }

        ESP_LOGI(kTag, "USB CDC-ACM device opened");
        configure_line_coding(*device);
        update_status(clear_status(), "clear sensor waiting status");

        while (xSemaphoreTake(device_disconnected_sem, 0) != pdTRUE) {
            RawSensorLine line;
            if (xQueueReceive(line_queue, &line, kLinePollDelay) == pdTRUE) {
                process_sensor_line(line, validator);
            }
        }

        ESP_LOGW(kTag, "USB device disconnected, reopening after delay");
        // 先关闭旧设备，停止接收回调，再清空旧连接的样本状态。
        device.reset();
        assembler.reset();
        validator.reset();
        vTaskDelay(kOpenRetryDelay);
    }
}
