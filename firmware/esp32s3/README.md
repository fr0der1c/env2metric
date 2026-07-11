# env2metric ESP32-S3 Firmware

这是 YN038 ESP32-S3 开发板上的正式采集固件。固件通过 USB Host 读取空气质量检测仪的 CDC-ACM 串口数据，解析成空气质量指标，并通过 OTLP HTTP 上报到 SigNoz。

## 当前行为

- 启动后初始化板载 RGB LED，数据脚为 `GPIO48`
- 连接 Wi-Fi，断开后无限重试
- Wi-Fi 连上后强制进行 SNTP 时间同步
- 时间同步成功后才进入 USB 检测仪采集流程
- 只打开已验证的检测仪 USB 设备：`vid=0x303a pid=0x1001 interface=0`
- 解析检测仪 CSV 数据并执行范围校验
- 通过 OTLP HTTP 上报真实空气质量指标
- 上报失败只记录 warning 并继续采集，不重启固件

## 指标

固件和 SigNoz 看板使用以下指标名：

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

OTLP 地址通过项目配置项设置。示例：

```text
http://<host>:4318/v1/metrics
```

相关配置项：

```text
CONFIG_ENV2METRIC_OTLP_METRICS_URL
CONFIG_ENV2METRIC_OTLP_HTTP_TIMEOUT_MS
CONFIG_ENV2METRIC_NTP_SERVER
```

## LED 灯语

- Wi-Fi 未连接或正在重连：蓝色快闪，亮 200ms / 灭 200ms
- 检测仪未连接或正在等待 USB 检测仪：黄色快闪，亮 200ms / 灭 200ms
- 读到有效样本并开始 OTLP 上报：白色，保持 1 秒
- OTLP 上报成功：白色，保持 1 秒
- OTLP 上报失败：橙色，保持 1 秒
- 样本解析或校验失败：黄色，保持 1 秒
- Wi-Fi 配置缺失、Wi-Fi 初始化失败等启动致命错误：红色常亮

成功上报路径会先显示 1 秒白灯表示开始上报，再显示 1 秒白灯表示成功，因此肉眼看到的是约 2 秒白灯。

## 构建环境

ESP-IDF 和工具链安装在 Git 仓库上一级的工作区内，避免污染全局环境：

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

当前组件管理器在本地构建中关闭，USB Host 相关组件已经放在 `firmware/esp32s3/components/`：

```bash
IDF_COMPONENT_MANAGER=0 idf.py -C firmware/esp32s3 build
```

## 烧录和日志

macOS 通常会将 YN038 CH340 识别为类似下面的串口：

```text
/dev/cu.usbserial-xxxx
```

烧录：

```bash
IDF_COMPONENT_MANAGER=0 idf.py -C firmware/esp32s3 -p /dev/cu.usbserial-xxxx flash
```

查看日志：

```bash
IDF_COMPONENT_MANAGER=0 idf.py -C firmware/esp32s3 -p /dev/cu.usbserial-xxxx monitor
```

退出 monitor 使用 `Ctrl+]`。
