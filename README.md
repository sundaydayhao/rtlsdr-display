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
- 支持运行时动态调频

### 3. AM 广播接收

- 接收 AM 广播（默认 909 kHz，可配置）
- 采样率 1.024 MHz，包络检波解调
- 输出 48 kHz 单声道 PCM 音频，通过 `QAudioSink` 实时播放
- 支持运行时动态调频

### 4. 无线传感器解码

- 接收 433/868 MHz 频段无线传感器信号（温湿度计、气象站等）
- 支持 OOK/ASK 调制解调
- 内置多协议解码器（基于 rtl_433 协议库）
- `SensorEventModel`（QAbstractListModel）供 QML 绑定，实时展示传感器数据
- `SensorDecodeView.qml`：列表视图显示传感器 ID、类型、温度、湿度、电池状态等

### 5. 频谱瀑布图

- 实时 FFT 频谱分析与瀑布图显示
- 采样率 2.4 MHz，FFT 大小 1024 点
- `WaterfallView.qml`：基于 QML Canvas 绘制滚动瀑布图，颜色映射信号强度
- 支持自定义中心频率

### 6. IQ 原始数据录制

- 将 RTL-SDR 原始 IQ 数据录制为二进制文件（`.bin`）
- 支持配置中心频率与采样率
- 可用于离线回放与分析

---

## 界面架构

采用 Qt6 QML 构建现代化触摸友好 UI，适合嵌入式触摸屏：

| QML 组件 | 说明 |
|---|---|
| `Main.qml` | 主窗口，应用磁贴导航 |
| `AppTile.qml` | 功能入口磁贴组件 |
| `FeaturePage.qml` | 功能页面基础框架 |
| `RadarMapView.qml` | ADS-B 雷达地图视图 |
| `WaterfallView.qml` | 频谱瀑布图视图 |
| `SensorDecodeView.qml` | 传感器数据列表视图 |
| `StatusPill.qml` | 状态指示胶囊组件 |
| `IconGlyph.qml` | 图标字形组件 |

---

## 技术架构

```
┌─────────────────────────────────────────────────────┐
│                    QML UI 层                         │
│  Main ─ AppTile ─ FeaturePage ─ RadarMap/Waterfall  │
└──────────────────────┬──────────────────────────────┘
                       │ Qt Properties / Signals
┌──────────────────────▼──────────────────────────────┐
│                  C++ 业务层                          │
│  AppLauncher（QML 注册中心）                         │
│  ├── AdsbReceiver  ──► AdsbDecoder ──► AircraftModel│
│  ├── FmReceiver    ──► QAudioSink                   │
│  ├── AmReceiver    ──► QAudioSink                   │
│  ├── SensorReceiver──► SensorDecoder ──► EventModel │
│  ├── WaterfallReceiver ──► FFT ──► QML Canvas       │
│  └── RecordReceiver──► 文件写入                     │
└──────────────────────┬──────────────────────────────┘
                       │ librtlsdr API
┌──────────────────────▼──────────────────────────────┐
│              RTL-SDR 硬件驱动层                      │
│              librtlsdr (rtlsdr_*)                   │
└─────────────────────────────────────────────────────┘
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

# AM 默认频率（Hz）
AM_FREQUENCY=909000

# 传感器接收频率（Hz）
SENSOR_FREQUENCY=433920000

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
│   ├── raspberrypi-buildroot-toolchain.cmake.example
│   └── rk3566-buildroot-toolchain.cmake.example
├── deploy/                     # 部署配置模板
│   ├── rtlsdr-panel.env.example
│   └── raspberrypi.env.example
├── qml/                        # QML UI 组件
│   ├── Main.qml
│   ├── AppTile.qml
│   ├── FeaturePage.qml
│   ├── RadarMapView.qml
│   ├── WaterfallView.qml
│   ├── SensorDecodeView.qml
│   ├── StatusPill.qml
│   └── IconGlyph.qml
├── resources/
│   └── qml.qrc                 # Qt 资源文件
├── src/
│   ├── main.cpp                # 程序入口
│   ├── AppLauncher.h/cpp       # QML 上下文注册
│   ├── adsb/                   # ADS-B 模块
│   │   ├── AdsbDecoder         # Mode S 报文解码
│   │   ├── AdsbReceiver        # RTL-SDR 接收线程
│   │   └── AdsbAircraftModel   # QML 数据模型
│   ├── fm/                     # FM 接收模块
│   ├── am/                     # AM 接收模块
│   ├── sensors/                # 无线传感器模块
│   │   ├── SensorDecoder       # 多协议解码
│   │   ├── SensorReceiver      # RTL-SDR 接收线程
│   │   └── SensorEventModel    # QML 数据模型
│   ├── waterfall/              # 频谱瀑布图模块
│   └── record/                 # IQ 数据录制模块
└── sdr-idea/                   # 方案文档与参考资料
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

## 参考资料

- [librtlsdr](https://github.com/osmocom/rtl-sdr)
- [rtl_433](https://github.com/merbanan/rtl_433)
- [ADS-B 协议规范（ICAO Annex 10）](https://www.icao.int/)
- [Qt6 文档](https://doc.qt.io/qt-6/)
- [`sdr-idea/`](sdr-idea/) — 项目方案分析与功能扩展文档