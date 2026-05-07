# RTL-SDR 嵌入式软件无线电平台

基于 RTL-SDR 硬件的嵌入式软件无线电（SDR）应用平台，使用 **C++17 + Qt6/QML** 构建，支持多种无线电接收与解码功能，适用于树莓派、RK3566 等嵌入式 Linux 设备。

---

## 功能模块

### 1. ADS-B 航空器追踪（1090 MHz）

- 接收并解码 ADS-B Mode S 报文（1090 MHz）
- 支持 DF17 长报文解析：位置（CPR 解码）、速度、航向、高度、呼号
- 支持 DF11 短报文（飞机能力公告）
- 内置 `AdsbAircraftModel`（QAbstractListModel）供 QML 直接绑定
- 雷达地图视图（`RadarMapView.qml`）：以极坐标方式实时显示周边航空器
- 航空器信息包含：ICAO 地址、呼号、经纬度、高度（ft）、速度（kt）、航向（°）、最后更新时间

### 2. FM 广播接收

- 接收标准 FM 广播（默认 100.7 MHz，可配置）
- 采样率 2.4 MHz，解调后输出 48 kHz 单声道 PCM 音频
- 通过 Qt Multimedia（`QAudioSink`）实时播放
- 支持运行时动态调频（步进按钮 / 直接输入）、WFM / NFM 模式切换、音量调节
- FM 页面显示当前调谐频点的实时 FFT 信号频谱，并展示平均能量、峰值强度、峰值频率、峰均差和音频电平

### 3. 无线传感器解码

- 接收 433/315/303 MHz 频段无线传感器信号（温湿度计、气象站、门磁等）
- 支持 OOK/ASK 调制解调
- 内置多协议解码器（Nexus、Baldr、Acurite、ThermoPro、HCS200 等）
- `SensorEventModel`（QAbstractListModel）供 QML 绑定，实时展示传感器数据
- `SensorDecodeView.qml`：列表视图显示传感器 ID、类型、温度、湿度、电池状态等

### 4. 频谱瀑布图

- 实时 FFT 频谱分析与瀑布图显示
- 采样率 2.4 MHz，FFT 大小 512 点（可通过环境变量配置 128–2048）
- `WaterfallView.qml`：基于 QML Canvas 绘制滚动瀑布图，颜色映射信号强度
- 支持运行时动态调频（步进按钮 / 直接输入）、频率标记线
- 环境变量配置：`RTLSDR_WATERFALL_FREQ`、`RTLSDR_WATERFALL_SAMPLE_RATE`、`RTLSDR_WATERFALL_FFT_SIZE`、`RTLSDR_WATERFALL_GAIN`

### 5. 宽带频谱扫描（热力图）

- 模拟 rtl_power 的多跳扫描方式，对大范围频段逐步扫描
- 每跳调谐 RTL-SDR → 采集 IQ → Cooley-Tukey FFT → 修复 DC 尖峰 → 拼接成完整扫描行
- 支持运行时动态修改扫描范围（原子变量，下一行生效）
- `HeatmapView.qml`：Canvas 滚动热力图，Jet 色彩映射（黑→蓝→青→绿→黄→红）
- 顶部渐变色标图例（弱→强），底部频率轴标签
- 内置预设频段：FM（87.5–108 MHz）、航空 VHF（108–137 MHz）、433 MHz ISM
- 自动降采样至 ≤1024 显示频点，保证渲染性能
- 环境变量配置：`RTLSDR_SCANNER_START_FREQ`、`RTLSDR_SCANNER_STOP_FREQ`、`RTLSDR_SCANNER_SAMPLE_RATE`、`RTLSDR_SCANNER_GAIN`

### 6. IQ 原始数据录制与回放

**录制模式：**
- 将 RTL-SDR 原始 IQ 数据录制为 uint8 二进制文件（`.iq` / `.bin`）
- 文件名自动包含中心频率和采样率（如 `iq_100000000Hz_2400000sps_20240101_120000.iq`）
- 支持配置中心频率、采样率、保存路径
- 实时显示文件大小、录制时长、保存路径

**回放模式：**
- 读取已录制的 `.iq` / `.bin` 文件，离线回放 IQ 数据
- 自动从文件名解析中心频率和采样率
- 通过 Cooley-Tukey FFT 计算频谱，以 `WaterfallView` 实时显示
- 显示回放进度条、已用时间 / 总时长、文件频率信息
- 文件末尾自动循环回放
- 侧边栏提供录制 / 回放模式切换按钮

---

## 界面架构

采用 Qt6 QML 构建现代化触摸友好 UI，适合嵌入式触摸屏：

| QML 组件 | 说明 |
|---|---|
| `Main.qml` | 主窗口，应用磁贴导航（6 个功能入口） |
| `AppTile.qml` | 功能入口磁贴组件 |
| `FeaturePage.qml` | 功能页面框架（左侧视图 + 右侧控制面板） |
| `RadarMapView.qml` | ADS-B 雷达地图视图 |
| `WaterfallView.qml` | 频谱瀑布图视图（也用于 IQ 回放） |
| `SpectrumView.qml` | 实时 FFT 信号频谱视图 |
| `HeatmapView.qml` | 宽带扫描热力图视图 |
| `SensorDecodeView.qml` | 传感器数据列表视图 |
| `StatusPill.qml` | 状态指示胶囊组件 |
| `IconGlyph.qml` | 图标字形组件 |

---

## 技术架构

```
┌──────────────────────────────────────────────────────┐
│                     QML UI 层                         │
│  Main ─ AppTile ─ FeaturePage ─ RadarMap/Waterfall   │
│                              └─ HeatmapView          │
└───────────────────────┬──────────────────────────────┘
                        │ Qt Properties / Signals
┌───────────────────────▼──────────────────────────────┐
│                   C++ 业务层                          │
│  AppLauncher（QML 注册中心）                          │
│  ├── AdsbReceiver    ──► AdsbDecoder ──► AircraftModel│
│  ├── FmReceiver      ──► QAudioSink                  │
│  ├── SensorReceiver  ──► SensorDecoder ──► EventModel│
│  ├── WaterfallReceiver ──► FFT ──► QML Canvas        │
│  ├── ScannerReceiver ──► 多跳FFT ──► HeatmapView     │
│  ├── RecordReceiver  ──► 文件写入（录制）             │
│  └── RtlPlaybackWorker ──► FFT ──► WaterfallView     │
└───────────────────────┬──────────────────────────────┘
                        │ librtlsdr API
┌───────────────────────▼──────────────────────────────┐
│               RTL-SDR 硬件驱动层                      │
│               librtlsdr (rtlsdr_*)                   │
└──────────────────────────────────────────────────────┘
```

---

## 依赖项

| 依赖 | 版本要求 | 说明 |
|---|---|---|
| Qt | ≥ 6.2 | Core、Quick、Multimedia、Positioning |
| librtlsdr | ≥ 0.6 | RTL-SDR 硬件驱动 |
| libusb | ≥ 1.0 | USB 设备访问 |
| CMake | ≥ 3.16 | 构建系统 |
| C++ 编译器 | C++17 | GCC / Clang |
| rtl_433（可选） | — | 传感器协议解码库 |

---

## 构建

### 本机构建

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

### 交叉编译（树莓派）

```bash
mkdir build-rpi && cd build-rpi
cmake .. \
  -DCMAKE_TOOLCHAIN_FILE=../cmake/raspberrypi-buildroot-toolchain.cmake.example \
  -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

### 交叉编译（RK3566）

```bash
mkdir build-rk3566 && cd build-rk3566
cmake .. \
  -DCMAKE_TOOLCHAIN_FILE=../cmake/rk3566-buildroot-toolchain.cmake.example \
  -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

---

## 部署

### 环境变量配置

复制并修改部署配置文件：

```bash
cp deploy/rtlsdr-panel.env.example deploy/rtlsdr-panel.env
cp deploy/raspberrypi.env.example deploy/raspberrypi.env
```

主要配置项（`rtlsdr-panel.env`）：

```env
# RTL-SDR 设备索引
RTLSDR_DEVICE_INDEX=0

# ADS-B 接收增益（dB）
ADSB_GAIN=40

# FM 默认频率（Hz）
FM_FREQUENCY=100700000

# 传感器接收频率（Hz）
SENSOR_FREQUENCY=433920000

# 宽带扫描范围（Hz）
RTLSDR_SCANNER_START_FREQ=88000000
RTLSDR_SCANNER_STOP_FREQ=108000000

# 录制输出目录
RECORD_OUTPUT_DIR=/tmp/rtlsdr
```

### 运行

```bash
./rtlsdr-panel
```

---

## 项目结构

```
rtlsdr/
├── CMakeLists.txt              # 构建配置
├── cmake/                      # 交叉编译工具链模板
├── deploy/                     # 部署配置模板
├── qml/                        # QML UI 组件
│   ├── Main.qml
│   ├── AppTile.qml
│   ├── FeaturePage.qml
│   ├── RadarMapView.qml
│   ├── WaterfallView.qml
│   ├── HeatmapView.qml
│   ├── SensorDecodeView.qml
│   ├── StatusPill.qml
│   └── IconGlyph.qml
├── resources/
│   └── qml.qrc                 # Qt 资源文件
└── src/
    ├── main.cpp                # 程序入口
    ├── AppLauncher.h/cpp       # QML 上下文注册
    ├── adsb/                   # ADS-B 模块
    │   ├── AdsbDecoder         # Mode S 报文解码
    │   ├── AdsbReceiver        # RTL-SDR 接收线程
    │   └── AdsbAircraftModel   # QML 数据模型
    ├── fm/                     # FM 接收模块
    ├── sensors/                # 无线传感器模块
    │   ├── SensorDecoder       # 多协议解码
    │   ├── SensorReceiver      # RTL-SDR 接收线程
    │   └── SensorEventModel    # QML 数据模型
    ├── waterfall/              # 频谱瀑布图模块
    ├── scanner/                # 宽带频谱扫描模块
    │   └── ScannerReceiver     # 多跳扫描 + FFT + 热力图数据
    └── record/                 # IQ 数据录制 + 回放模块
        └── RecordReceiver      # 录制（RtlRecordWorker）+ 回放（RtlPlaybackWorker）
```

---

## 支持的硬件

- **RTL-SDR Blog V3 / V4**（推荐）
- 任何基于 RTL2832U + R820T/R820T2/R828D 芯片的 DVB-T 棒
- 目标平台：树莓派 3/4/5、RK3566 开发板及其他运行 Linux 的嵌入式设备

---

## 许可证

本项目代码采用 [MIT License](LICENSE) 开源。

---

## 可扩展功能

以下功能尚未实现，但基于现有架构可以较低成本扩展，详见 [`sdr-idea/`](sdr-idea/) 目录中的方案文档。

### 无线电接收与解码

| 功能 | 频段 | 说明 |
|---|---|---|
| **NOAA 气象卫星图像** | 137 MHz | 接收 NOAA-15/18/19 APT 信号，解码实时气象云图 |
| **DAB+ 数字广播** | 174–240 MHz | 数字音频广播接收与解码 |
| **ACARS 航空数据链** | 129.125 / 136.9 MHz | 解码飞机与地面站之间的数字数据报文 |
| **VDL Mode 2** | 136 MHz | 航空 VHF 数字链路，补充 ACARS |
| **POCSAG 寻呼机** | 152 / 466 MHz | 解码无线寻呼消息 |
| **APRS 业余无线电** | 144.39 / 144.8 MHz | 自动位置报告系统，解码 GPS 位置与消息 |
| **P25 数字对讲** | 136–512 MHz | 公共安全数字语音通信解码 |
| **DMR 数字对讲** | 136–512 MHz | 数字移动无线电语音/数据解码 |
| **AIS 船舶追踪** | 161.975 / 162.025 MHz | 解码船舶自动识别系统报文，显示海图 |
| **WSPR / FT8** | HF 多频段 | 业余无线电弱信号传播报告解码 |
| **SSB / CW 接收** | HF 全频段 | 单边带与摩尔斯电码接收 |

### 导航与定位

| 功能 | 频段 | 说明 |
|---|---|---|
| **GPS/北斗软件接收机** | 1575.42 / 1561.098 MHz | 软件解码 GPS L1 / 北斗 B1 信号，输出 NMEA |
| **VOR 导航台解码** | 108–118 MHz | 甚高频全向信标方位角解码 |
| **ILS 仪表着陆** | 108–112 MHz | 航向道与下滑道信号解码 |

### 物联网与工业

| 功能 | 频段 | 说明 |
|---|---|---|
| **LoRa 数据包嗅探** | 433 / 868 / 915 MHz | 监听 LoRaWAN 上行数据包 |
| **电表/水表无线抄表** | 433 / 868 MHz | 解码 M-Bus / wM-Bus 无线抄表报文 |
| **TPMS 胎压监测** | 315 / 433 MHz | 解码汽车胎压传感器数据 |

### 平台与工程扩展

| 功能 | 说明 |
|---|---|
| **IQ 回放增强** | 支持 SigMF 元数据格式、可变回放速度、跳转定位 |
| **Web 远程控制** | 通过 WebSocket 将频谱/解码数据推送到浏览器，实现远程监控 |
| **MQTT 数据上报** | 将传感器、ADS-B、AIS 等解码数据发布到 MQTT Broker |
| **数据库存储** | 将航空器轨迹、传感器历史数据存入 SQLite/InfluxDB |
| **多设备网络聚合** | 类似 FlightAware/ADS-B Exchange，聚合多节点数据 |
| **Docker 容器化部署** | 打包为 Docker 镜像，简化嵌入式 Linux 部署 |

> 详细方案分析请参阅 [`sdr-idea/RTLSDR_快速落地项目与变现分析.md`](sdr-idea/RTLSDR_快速落地项目与变现分析.md) 与 [`sdr-idea/RTLSDR嵌入式设备方案与功能扩展总结.md`](sdr-idea/RTLSDR嵌入式设备方案与功能扩展总结.md)。

---

## 参考资料

- [librtlsdr](https://github.com/osmocom/rtl-sdr)
- [rtl_433](https://github.com/merbanan/rtl_433)
- [ADS-B 协议规范（ICAO Annex 10）](https://www.icao.int/)
- [Qt6 文档](https://doc.qt.io/qt-6/)
- [`sdr-idea/`](sdr-idea/) — 项目方案分析与功能扩展文档
