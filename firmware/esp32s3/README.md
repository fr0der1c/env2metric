# env2metric ESP32-S3 Firmware

这是 YN038 ESP32-S3 开发板上的正式采集固件。固件通过 USB Host 读取空气质量检测仪的 CDC-ACM 串口数据，解析成空气质量指标，并通过 OTLP HTTP 上报到 OneUptime。

## 当前行为

- 启动后初始化板载 RGB LED，数据脚为 `GPIO48`
- 连接 Wi-Fi，断开后无限重试
- Wi-Fi 连上后强制进行 SNTP 时间同步
- 时间同步成功后才进入 USB 检测仪采集流程
- 只打开已验证的检测仪 USB 设备：`vid=0x303a pid=0x1001 interface=0`
- 解析检测仪 CSV 数据并执行范围校验
- 通过 OTLP HTTP 上报真实空气质量指标
- 上报失败只记录 warning 并继续采集，不重启固件
- USB 断开后清空残留半行和序号记录，重连后允许检测仪序号重新开始
- 检测仪每连续连接 7 天后通过 USB 复位一次，不重启 ESP32-S3 网关

## 检测仪定期重启

按检测仪厂商针对 VOC 传感器基线漂移的维护建议，固件使用 `esp_timer_get_time()` 的 64 位单调时间，每连续连接 7×24 小时尝试一次 USB 复位，不依赖 SNTP 时间、时区或固定星期。ESP32-S3 重启或检测仪断开重连后，周期重新计时。复位检查在采集循环中执行；若正在上传样本，会在本次处理结束后检查。

复位采用已在电脑直连及 ESP32-S3 USB Host 上验证的正常启动控制线时序：先释放 RTS、清除 DTR，等待 100ms，再置位 RTS 200ms 后释放，保持 DTR 未置位，避免进入下载模式。复位期间暂停数据拼接，清空待处理样本，并在恢复读取前重置序号校验。ESP32-S3 USB Host 实测中，检测仪在重启约 30 秒后恢复输出，随后自动恢复上报。

复位后 USB 仍在线时保留设备句柄，避免关闭再打开触发额外重启；实际断开时沿用重连流程。控制传输失败时记录 warning，也会尝试释放已置位的 RTS；不立即补发复位请求，连接仍在线时下一次尝试在 7 天后。接收回调与缓冲区清理通过互斥锁同步。

## 指标

固件和 OneUptime 看板使用以下指标名：

```text
env2metric.air.particle_0_3um
env2metric.air.pm25
env2metric.air.hcho
env2metric.air.co2
env2metric.air.temperature
env2metric.air.humidity
env2metric.air.voc
env2metric.air.sequence
```

VOC 按检测仪原始 `ppb` 值换算成 `ppm` 后上报。每个 datapoint 都带 `timeUnixNano`，时间来自启动后的 SNTP 同步。

## 配置

Wi-Fi 使用 ESP-IDF 项目配置项：

```text
CONFIG_ENV2METRIC_WIFI_SSID
CONFIG_ENV2METRIC_WIFI_PASSWORD
```

真实 SSID 和密码只写入本地文件：

```text
firmware/esp32s3/sdkconfig.defaults.local
```

示例：

```text
CONFIG_ENV2METRIC_WIFI_SSID="your-ssid"
CONFIG_ENV2METRIC_WIFI_PASSWORD="your-password"
```

`sdkconfig.defaults.local` 和生成的 `sdkconfig` 会包含本地 Wi-Fi 密码，已经加入 `.gitignore`，不要提交到 Git。

OTLP 地址和可选请求头通过项目配置项设置。请求头使用与 `OTEL_EXPORTER_OTLP_HEADERS` 一致的 `名称=值` 格式，多个 Header 用逗号分隔。OneUptime 使用 Telemetry Ingestion Key：

```text
https://<host>/otlp/v1/metrics
x-oneuptime-token=<Telemetry Ingestion Key>
```

相关配置项：

```text
CONFIG_ENV2METRIC_OTLP_METRICS_URL
CONFIG_ENV2METRIC_OTLP_HEADERS
CONFIG_ENV2METRIC_OTLP_HTTP_TIMEOUT_MS
CONFIG_ENV2METRIC_SNTP_SERVER
```

## LED 灯语

- Wi-Fi 未连接或正在重连：蓝色快闪，亮 200ms / 灭 200ms
- 检测仪未连接或正在等待 USB 检测仪：黄色快闪，亮 200ms / 灭 200ms
- 读到有效样本并开始 OTLP 上报：白色，保持 0.5 秒
- OTLP 上报成功：白色，保持 0.5 秒
- OTLP 上报失败：橙色，保持 0.5 秒
- 样本解析或校验失败：黄色，保持 0.5 秒
- Wi-Fi 配置缺失、Wi-Fi 初始化失败等启动致命错误：红色常亮

成功上报时，开始上传前和收到成功响应后各亮一次白灯，每次 0.5 秒。两次白灯之间可能有灭灯间隔，间隔取决于上传耗时。

## 构建环境

使用 ESP-IDF v5.5.4，与依赖锁文件及已完成的实机验证版本一致。

ESP-IDF 安装步骤见仓库根目录 README。也可以将 ESP-IDF 和工具链安装在 Git 仓库上一级的工作区内：

```text
../third_party/esp-idf
../.espressif
```

从 Git 仓库根目录激活环境：

```bash
ENV2METRIC_WORKSPACE="$(cd .. && pwd)"
export IDF_TOOLS_PATH="$ENV2METRIC_WORKSPACE/.espressif"
. "$ENV2METRIC_WORKSPACE/third_party/esp-idf/export.sh"
```

使用 ESP-IDF 官方组件管理器获取 `led_strip`、`usb`、`usb_host_cdc_acm` 和 `usb_host_vcp`。版本在 `main/idf_component.yml` 中声明，并由已提交的 `dependencies.lock` 锁定。首次构建需要联网，组件自动下载到被 Git 忽略的 `managed_components/`，无需手动解包或修改 ESP-IDF 源码。

```bash
unset IDF_COMPONENT_MANAGER
idf.py -C firmware/esp32s3 set-target esp32s3
idf.py -C firmware/esp32s3 build
```

`set-target` 仅用于首次构建或切换芯片；后续直接运行 `build`。从原来的手动组件布局迁移时，先执行 `idf.py -C firmware/esp32s3 fullclean`，再运行 `build`，保留已有的 `sdkconfig` 和 `sdkconfig.defaults.local`。

在 Codex 等受限沙箱中，组件管理器可能因无法查询父进程而报 `PermissionError`。此时应授予构建所需的进程和网络访问权限，不要通过关闭组件管理器绕过依赖下载。

## 序号校验回归测试

从 Git 仓库根目录运行，验证重连后接受小序号，同时保留连接内的递增校验和样本范围校验：

```bash
c++ -std=c++17 -Wall -Wextra -Werror -I firmware/esp32s3/main \
  firmware/esp32s3/main/air_quality_validator.cpp \
  firmware/esp32s3/tests/air_quality_validator_test.cpp \
  -o /tmp/env2metric-air-quality-validator-test
/tmp/env2metric-air-quality-validator-test
```

## 定期重启回归测试

使用模拟的单调时间验证七天触发边界、同一周期仅尝试一次、错过多个周期不补发，以及长期运行和断线重连后的计时。测试不操作 USB 设备，也无需实际等待七天：

```bash
c++ -std=c++17 -Wall -Wextra -Werror -I firmware/esp32s3/main \
  firmware/esp32s3/tests/sensor_restart_schedule_test.cpp \
  -o /tmp/env2metric-sensor-restart-schedule-test
/tmp/env2metric-sensor-restart-schedule-test
```

## 烧录和日志

macOS 通常会将 YN038 CH340 识别为类似下面的串口：

```text
/dev/cu.usbserial-xxxx
```

烧录：

```bash
idf.py -C firmware/esp32s3 -p /dev/cu.usbserial-xxxx flash
```

查看日志：

```bash
idf.py -C firmware/esp32s3 -p /dev/cu.usbserial-xxxx monitor
```

退出 monitor 使用 `Ctrl+]`。
