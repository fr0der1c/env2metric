# env2metric

`env2metric` 把正和清源空气质量检测仪B39变成一台可以长期运行的网络传感器：ESP32-S3 开发板通过 USB 读取检测仪数据，再使用 OpenTelemetry OTLP/HTTP 上传到 SigNoz。

运行后可以在 Dashboard 中查看 PM2.5、CO2、甲醛、VOC、温度和湿度的当前值、历史趋势及空气质量参考线。

## 准备硬件

本项目已经在下面这套硬件上完成验证：

- [正和清源空气质量检测仪 B39](https://item.taobao.com/item.htm?id=532126167695)
- [YN038 ESP32-S3 开发板](https://item.taobao.com/item.htm?id=890396470166)
- 一根用于连接开发板与电脑的 USB 数据线
- 检测仪随附的 USB 连接线

还需要：

- 一个 2.4 GHz Wi-Fi 网络
- 一套可从该 Wi-Fi 访问的 SigNoz
- 一台用于编译和刷机的 macOS 或 Linux 电脑

## 连接设备

1. 将空气质量检测仪接到 YN038 的 USB Host 接口。
2. 将 YN038 的 CH340 下载/调试接口接到电脑。
3. 确认检测仪已经通电。

开发板刷入固件后不再依赖电脑运行。电脑只在首次刷机或查看串口日志时需要连接。

## 快速开始

### 1. 安装 ESP-IDF

项目使用 ESP-IDF 5.5。下面是 macOS/Linux 的隔离安装方式，不会替换系统编译器：

```bash
mkdir -p ~/esp
cd ~/esp
git clone --recursive --branch v5.5.3 https://github.com/espressif/esp-idf.git
cd esp-idf
./install.sh all
```

每次打开新的终端后激活 ESP-IDF：

```bash
. "$HOME/esp/esp-idf/export.sh"
```

### 2. 配置 Wi-Fi 和 SigNoz

回到 env2metric 仓库根目录，创建只在本机使用的配置文件：

```bash
cp firmware/esp32s3/sdkconfig.defaults.local.example \
  firmware/esp32s3/sdkconfig.defaults.local
```

编辑 `firmware/esp32s3/sdkconfig.defaults.local`：

```text
CONFIG_ENV2METRIC_WIFI_SSID="your-wifi-name"
CONFIG_ENV2METRIC_WIFI_PASSWORD="your-wifi-password"
CONFIG_ENV2METRIC_OTLP_METRICS_URL="http://your-signoz-host:4318/v1/metrics"
```

该文件包含 Wi-Fi 密码，已经被 Git 忽略，不要提交它。

### 3. 编译固件

```bash
IDF_COMPONENT_MANAGER=0 idf.py -C firmware/esp32s3 build
```

### 4. 刷入开发板

先找到开发板串口。macOS 通常是 `/dev/cu.usbserial-*`，Linux 通常是 `/dev/ttyUSB*`。

```bash
IDF_COMPONENT_MANAGER=0 idf.py -C firmware/esp32s3 \
  -p /dev/cu.usbserial-xxxx flash monitor
```

把端口替换成实际设备。退出日志监视器使用 `Ctrl+]`。

### 5. 查看数据

启动成功后，固件会依次连接 Wi-Fi、同步时间、等待检测仪数据并上传指标。SigNoz 中出现 `env2metric.air.*` 指标，就表示整条链路已经跑通。

服务端暂时不可用不会导致设备卡死或程序退出。一次数据上传失败后，固件会继续读取并上报后续数据。

## 状态灯

| 灯光 | 含义 |
| --- | --- |
| 蓝色快闪 | Wi-Fi 未连接或正在重连 |
| 黄色快闪 | 等待空气质量检测仪 |
| 白色约 2 秒 | 已读取有效数据并上传成功 |
| 橙色 1 秒 | 本次上传失败，固件将继续运行 |
| 黄色 1 秒 | 本次数据解析或校验失败 |
| 红色常亮 | 启动发生致命错误 |

## 创建 SigNoz Dashboard

仓库提供了中文 Dashboard 模板，包含六个当前值面板、独立量纲趋势图和空气质量参考线。

```bash
export SIGNOZ_BASE_URL=http://your-signoz-host:8080
export SIGNOZ_TOKEN=<SigNoz AUTH_TOKEN>
./scripts/create_signoz_dashboard.sh
```

如果 API 返回 `unauthenticated`，通常是 token 已过期，需要从登录后的 SigNoz 浏览器会话中重新获取 `AUTH_TOKEN`。

Dashboard 模板位于 [dashboards/env2metric-air-quality.json](dashboards/env2metric-air-quality.json)。

## 采集指标

| 指标名 | 含义 | 单位 |
| --- | --- | --- |
| `env2metric.air.particle_0_3um` | 0.3 微米以上颗粒数 | `pcs/0.1L` |
| `env2metric.air.pm25` | PM2.5 | `µg/m³` |
| `env2metric.air.hcho` | 甲醛 | `µg/m³` |
| `env2metric.air.co2` | 二氧化碳 | `ppm` |
| `env2metric.air.temperature` | 温度 | `°C` |
| `env2metric.air.humidity` | 相对湿度 | `%` |
| `env2metric.air.voc` | VOC | `ppm` |
| `env2metric.air.sequence` | 检测仪输出序号 | `1` |

检测仪输出的 VOC 原始值按 `ppb` 解析，上传时换算为 `ppm`：

```text
1 ppm = 1000 ppb
```

固件会过滤明显异常的样本。例如 CO2 低于 `300 ppm` 时，日志会打印该组完整数据，但不会上传。

## Dashboard 参考范围

甲醛、VOC 和 CO2 使用五档颜色展示：绿色为优、蓝色为良、黄色为轻度污染、橙色为中度污染、红色为重度污染。

| 指标 | 优 | 良 | 轻度污染 | 中度污染 | 重度污染 |
| --- | --- | --- | --- | --- | --- |
| 甲醛 (`µg/m³`) | `0-80` | `80-120` | `120-200` | `200-400` | `400-800` |
| VOC (`ppm`) | `<0.22` | `<0.44` | `<0.88` | `<1.76` | `<2.64` |
| CO2 (`ppm`) | `<700` | `<1000` | `<1500` | `<2000` | `<3000` |

PM2.5 当前标注的参考限值为 `50 µg/m³`。温度舒适区间为 `18-25.5 °C`，湿度舒适区间为 `40-60%`。

## 项目结构

```text
env2metric/
  firmware/esp32s3/  # YN038 ESP32-S3 固件
  dashboards/        # SigNoz Dashboard 模板
  scripts/           # Dashboard 创建脚本
```

设备协议参数、固件配置和开发说明见 [firmware/esp32s3/README.md](firmware/esp32s3/README.md)。
