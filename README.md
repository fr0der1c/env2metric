# env2metric

`env2metric` 把正和清源空气质量检测仪B39变成一台可以长期运行的网络传感器：ESP32-S3 开发板通过 USB 读取检测仪数据，再使用 OpenTelemetry OTLP/HTTP 上传到 OneUptime。

运行后可以在 Dashboard 中查看 PM2.5、CO2、甲醛、VOC、温度和湿度的当前值、历史趋势及空气质量参考线。

## 准备硬件

本项目已经在下面这套硬件上完成验证：

- [正和清源空气质量检测仪 B39](https://item.taobao.com/item.htm?id=532126167695)
- [YN038 ESP32-S3 开发板](https://item.taobao.com/item.htm?id=890396470166)
- 一根用于连接开发板与电脑的 USB 数据线
- 检测仪随附的 USB 连接线

还需要：

- 一个 2.4 GHz Wi-Fi 网络
- 一套可从该 Wi-Fi 访问的 OneUptime
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

### 2. 配置 Wi-Fi 和 OneUptime

回到 env2metric 仓库根目录，创建只在本机使用的配置文件：

```bash
cp firmware/esp32s3/sdkconfig.defaults.local.example \
  firmware/esp32s3/sdkconfig.defaults.local
```

编辑 `firmware/esp32s3/sdkconfig.defaults.local`：

```text
CONFIG_ENV2METRIC_WIFI_SSID="your-wifi-name"
CONFIG_ENV2METRIC_WIFI_PASSWORD="your-wifi-password"
CONFIG_ENV2METRIC_OTLP_METRICS_URL="https://your-oneuptime-host/otlp/v1/metrics"
CONFIG_ENV2METRIC_OTLP_HEADERS="x-oneuptime-token=your-oneuptime-telemetry-ingestion-key"
```

`CONFIG_ENV2METRIC_OTLP_HEADERS` 与 `OTEL_EXPORTER_OTLP_HEADERS` 使用相同的 `名称=值` 格式，支持用逗号分隔多个 Header。OneUptime 的 Telemetry Ingestion Key 应填写为 `x-oneuptime-token=<key>`；本地 SigNoz 不需要鉴权时可留空。该文件包含 Wi-Fi 密码和遥测密钥，已经被 Git 忽略，不要提交它。

自托管 OneUptime 时，地址通常是 `https://your-oneuptime-host/otlp/v1/metrics`。请让 ESP32 信任该 HTTPS 证书；如暂时使用内网 HTTP，应使用对应的 `http://` 地址。

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

启动成功后，固件会依次连接 Wi-Fi、同步时间、等待检测仪数据并上传指标。OneUptime 的 Metrics 中出现 `env2metric.air.*` 指标，就表示整条链路已经跑通。

服务端暂时不可用不会导致设备卡死或程序退出。一次数据上传失败后，固件会继续读取并上报后续数据。

## 状态灯

| 灯光 | 含义 |
| --- | --- |
| 蓝色快闪 | Wi-Fi 未连接或正在重连 |
| 黄色快闪 | 等待空气质量检测仪 |
| 白色短亮 0.5 秒 | 开始上传有效数据或上传成功时各短亮一次 |
| 橙色短亮 0.5 秒 | 本次上传失败，固件将继续运行 |
| 黄色短亮 0.5 秒 | 本次数据解析或校验失败，跳过该样本并继续读取 |
| 红色常亮 | 启动发生致命错误 |

## 创建 OneUptime Dashboard

脚本会创建一个包含六个当前值面板和七个趋势图的 OneUptime Dashboard。它使用 OneUptime API Key，区别于设备上报所用的 Telemetry Ingestion Key。

```bash
./scripts/create_oneuptime_dashboard.sh
```

脚本会依次询问 OneUptime 地址、API Key 和 Project ID；API Key 输入时不会回显。也可预先设置同名的 `ONEUPTIME_BASE_URL`、`ONEUPTIME_API_KEY`、`ONEUPTIME_PROJECT_ID` 环境变量，适合自动化运行。同一项目中已有同名 Dashboard 时，脚本会先删除它再创建，API Key 因而需要 Dashboard 的创建与删除权限。

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
  dashboards/        # SigNoz Dashboard 模板（保留供旧部署使用）
  scripts/           # SigNoz 和 OneUptime Dashboard 创建脚本
```

设备协议参数、固件配置和开发说明见 [firmware/esp32s3/README.md](firmware/esp32s3/README.md)。

## 许可证

本项目以源码公开、限非商业使用的方式分享，不属于 OSI 定义的开源软件。各部分分别适用以下许可证，不是对同一内容提供任选许可：

| 内容 | 许可证 |
| --- | --- |
| 原创固件、脚本、测试、Dashboard 配置及构建配置 | [PolyForm Noncommercial 1.0.0](LICENSES/PolyForm-Noncommercial-1.0.0.txt) |
| 原创外壳设计（SCAD、STL、图片）及项目文档 | [CC BY-NC-SA 4.0](LICENSES/CC-BY-NC-SA-4.0.txt) |
| 第三方组件 | 保留各自的许可证及版权声明 |

外壳和文档的署名为 `frederic / env2metric`。分享时须保留署名、提供许可证链接并说明修改；分享改编版本时须遵守相同方式共享条款。

非商业用途不等同于免费提供，具体许可范围以完整条款为准。Espressif USB 组件继续使用其 Apache-2.0 许可证，项目的非商业限制不适用于这些第三方组件本身。完整许可范围见 [LICENSE](LICENSE)。
