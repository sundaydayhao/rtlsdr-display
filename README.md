# RTL-SDR Panel

这是一个面向 RK3566 buildroot、Raspberry Pi 和本机 Linux 的 Qt Quick/CMake 应用骨架。当前版本提供主界面、六个应用入口、内置 RTL-SDR ADS-B 解码、ADS-B 地图显示、RTL-SDR 实时频谱瀑布图、433/315 MHz 传感器/遥控钥匙报文解码、内置 AM/FM 调谐/解调播放、内置 I/Q 录制，以及一个 C++ 启动后端。

## 为什么用 Qt Quick

Qt 是 RK3566 这类嵌入式 Linux 板子上比较稳妥的 GUI 选择。Qt 官方的嵌入式 Linux 文档说明，EGLFS 可以在没有 X11/Wayland 的情况下直接使用 EGL/OpenGL ES，并且常用于带 GPU 的现代嵌入式设备。频谱瀑布图、地图、航迹标注这类动态画面更适合 Qt Quick/QML，而不是传统 Qt Widgets。

Gqrx 仍然很适合作为功能架构参考：它是基于 GNU Radio 和 Qt 的开源 SDR 接收器，具备 AM/FM/SSB 解调、FFT、瀑布图、I/Q 录制、远程控制等能力。ADS-B 部分参考 dump1090 的分层：RTL-SDR 采样、Mode S 前导码检测、PPM 比特判决、CRC 校验、DF17 扩展报文解析、CPR 经纬度解码，再把飞机状态推给 Qt 地图界面。

参考：

- Gqrx: https://github.com/gqrx-sdr/gqrx
- Gqrx 官网: https://www.gqrx.dk/
- dump1090: https://github.com/antirez/dump1090
- Qt Embedded Linux / EGLFS: https://doc.qt.io/qt-6/embedded-linux.html
- Qt Quick Scene Graph: https://doc.qt.io/qt-6.9/qtquick-visualcanvas-scenegraph.html
- Qt Location Map: https://doc.qt.io/qt-6/qml-qtlocation-map.html

## 本机编译

需要安装 CMake、C++ 编译器、Qt5 或 Qt6 的 Qt Quick/Network 开发包、`librtlsdr-dev`，以及用于默认 AM/FM 播放的 `aplay`。

Ubuntu 22.04 示例：

```sh
sudo apt update
sudo apt install -y build-essential cmake qtbase5-dev qtdeclarative5-dev librtlsdr-dev alsa-utils
```

```sh
cmake -S . -B build
cmake --build build -j
./build/rtlsdr-panel
```

如果在桌面 Linux 上运行，一般默认使用 xcb/wayland 平台插件即可。也可以临时指定：

```sh
QT_QPA_PLATFORM=xcb ./build/rtlsdr-panel
```

ADS-B 默认参数：

```sh
export RTLSDR_ADSB_DEVICE=0
export RTLSDR_ADSB_FREQ=1090000000
export RTLSDR_ADSB_SAMPLE_RATE=2000000
export RTLSDR_ADSB_PPM=0
# export RTLSDR_ADSB_GAIN=496
```

打开界面后进入 `ADS-B 飞机识别`，点击 `启动` 即可从 RTL-SDR 接收 1090 MHz ADS-B。地图固定使用高德在线瓦片，飞机坐标按 ADS-B 解码得到的 WGS84 经纬度转换为 GCJ-02 后叠加；界面会显示呼号/ICAO、高度、速度、航向、位置和最近更新时间。高度显示为米，速度显示为 km/h。点击右侧航班信息会把地图切到对应飞机居中，右侧 `航迹` 开关可控制是否显示历史轨迹。地图中心默认使用上海坐标，也可以改为你的接收站坐标：

```sh
export RTLSDR_PANEL_LAT=31.2304
export RTLSDR_PANEL_LON=121.4737
export RTLSDR_PANEL_MAP_AUTO_CENTER=1
```

应用会为地图请求设置 `rtlsdr-panel` User-Agent，并把瓦片缓存在 Qt cache 目录。

频谱瀑布图默认参数：

```sh
export RTLSDR_WATERFALL_DEVICE=0
export RTLSDR_WATERFALL_FREQ=100000000
export RTLSDR_WATERFALL_SAMPLE_RATE=2400000
export RTLSDR_WATERFALL_PPM=0
# export RTLSDR_WATERFALL_GAIN=496
# export RTLSDR_WATERFALL_FFT_SIZE=512
```

打开 `频谱瀑布图` 后可以直接输入中心频率 MHz，也可以用 `-10/-1/+1/+10` MHz 步进调谐。应用会按 RTL-SDR tuner 类型限制可调范围；常见 R820T/R820T2/R828D/R860 设备默认显示 24 MHz 到 1766 MHz。如果你的硬件支持直采或有扩展频段，可以用 `RTLSDR_WATERFALL_MIN_FREQ`、`RTLSDR_WATERFALL_MAX_FREQ` 和 `RTLSDR_WATERFALL_DIRECT_SAMPLING` 覆盖。

433/315 MHz 报文解码默认参数：

```sh
export RTLSDR_SENSOR_DEVICE=0
export RTLSDR_SENSOR_FREQ=433920000
export RTLSDR_SENSOR_SAMPLE_RATE=250000
export RTLSDR_SENSOR_PPM=0
# export RTLSDR_SENSOR_GAIN=496
# export RTLSDR_SENSOR_DIRECT_SAMPLING=0
```

打开 `433/315 报文解码` 后可以在 433.92 MHz、315.00 MHz 和 303.40 MHz 之间快速切换，也可以直接输入 MHz 频率。当前内置 OOK/ASK 脉冲切帧、常见 PPM 温湿度协议解析、EV1527/SC226x 风格 PWM 安防报文解析，以及汽车遥控钥匙/RKE 的被动识别。已支持 Nexus/FreeTec/Solight/TFA、Baldr E0666TH、Acurite 609TXC、ThermoPro TX2、Generic Remote、Kerui/Chuango 门磁/PIR/水浸、Microchip HCS200/HCS300、HCS361/KeeLoq、Chrysler 和 Audiovox PRO-OE3B 这类报文。Keeloq/滚动码只显示序列号、按钮位、电池状态和 encrypted/rolling 原始片段，不做重放、破解或密钥推导；未识别型号会优先从重复 burst 中抽取 48/64/66/67/96/128 bit 等固定长度 RKE 原始窗口，减少按键时长、重复帧和截断造成的 raw 长度抖动。

AM 默认参数：

```sh
export RTLSDR_AM_DEVICE=0
export RTLSDR_AM_FREQ=118000000
export RTLSDR_AM_SAMPLE_RATE=240000
export RTLSDR_AM_AUDIO_RATE=48000
export RTLSDR_AM_PPM=0
# export RTLSDR_AM_GAIN=496
# export RTLSDR_AM_VOLUME=0.80
# export RTLSDR_AM_BANDWIDTH=12000
# export RTLSDR_AM_AUDIO_CUTOFF=5000
```

打开 `AM 接收` 后可以输入 MHz 频率、用 `-0.1/-0.01/+0.01/+0.1` MHz 步进调谐。后端会从 RTL-SDR 读取 I/Q 数据，做 AM 包络检波、低通抽取、去直流、AGC 和单声道 PCM 输出。默认频率 118 MHz 适合普通 tuner 覆盖的航空 AM；如果要收中短波，需要硬件支持直采或上变频，并按实际硬件设置：

```sh
export RTLSDR_AM_DIRECT_SAMPLING=2
export RTLSDR_AM_FREQ=7200000
export RTLSDR_AM_MIN_FREQ=100000
export RTLSDR_AM_MAX_FREQ=30000000
```

FM 默认参数：

```sh
export RTLSDR_FM_DEVICE=0
export RTLSDR_FM_FREQ=100700000
export RTLSDR_FM_MODE=wfm
export RTLSDR_FM_AUDIO_RATE=48000
export RTLSDR_FM_PPM=0
# export RTLSDR_FM_GAIN=496
# export RTLSDR_FM_VOLUME=0.80
# export RTLSDR_FM_WFM_SAMPLE_RATE=1200000
# export RTLSDR_FM_NFM_SAMPLE_RATE=240000
```

打开 `FM 接收` 后可以输入 MHz 频率、用 `-1/-0.1/+0.1/+1` MHz 步进调谐，并在 WFM 广播和 NFM 窄带语音之间切换。后端会从 RTL-SDR 读取 I/Q 数据，做 FM 鉴频、低通抽取、去直流和单声道 PCM 输出。默认音频输出命令是：

```sh
aplay -q -t raw -f S16_LE -r 48000 -c 1
```

如果目标系统不用 `aplay`，可以设置 `RTLSDR_AM_AUDIO_CMD` 或 `RTLSDR_FM_AUDIO_CMD`，命令里可用 `{rate}`/`%r` 和 `{channels}`/`%c` 占位：

```sh
export RTLSDR_AM_AUDIO_CMD="aplay -q -t raw -f S16_LE -r {rate} -c {channels}"
export RTLSDR_FM_AUDIO_CMD="aplay -q -t raw -f S16_LE -r {rate} -c {channels}"
```

I/Q 录制默认参数：

```sh
export RTLSDR_RECORD_DEVICE=0
export RTLSDR_RECORD_FREQ=100000000
export RTLSDR_RECORD_SAMPLE_RATE=2400000
export RTLSDR_RECORD_PPM=0
# export RTLSDR_RECORD_GAIN=496
# export RTLSDR_RECORD_OUTPUT_DIR=/data/rtlsdr-recordings
# export RTLSDR_RECORD_FILE=/data/rtlsdr-recordings/capture.iq
```

打开 `录制射频信号` 后可以设置中心频率、采样率和保存路径。点击 `启动` 后应用会把 RTL-SDR 原始 `u8` 交错 I/Q 字节写入 `.iq` 文件，并在界面上显示录制时长、文件大小和实际保存位置。未配置 `RTLSDR_RECORD_FILE` 时，应用会在文档目录下的 `rtlsdr-recordings` 文件夹自动生成文件名；如果目标文件已存在，会自动追加序号避免覆盖。

### ADS-B 硬件要求

1090 MHz ADS-B 需要 RTL-SDR 的 tuner 能调到 1090 MHz。常见可用型号是 R820T/R820T2/R828D/R860 类 RTL-SDR。Fitipower FC0012、FCI FC2580 这类老 tuner 通常无法覆盖 1090 MHz；如果日志里出现下面内容，说明需要更换 RTL-SDR 硬件：

```text
Found Fitipower FC0012 tuner
[FC0012] no valid PLL combination found for 1090000000 Hz!
```

可用 `RTLSDR_PANEL_TARGET` 指定目标平台，用来打开平台相关的编译标识和界面平台提示：

```sh
cmake -S . -B build-rpi -DRTLSDR_PANEL_TARGET=raspberrypi
cmake --build build-rpi -j
```

目前支持：

- `auto`：默认值，按 Qt/CPU 宏自动显示平台。
- `desktop`：本机桌面 Linux。
- `rk3566`：RK3566 buildroot。
- `raspberrypi`：Raspberry Pi。

## RK3566 buildroot 交叉编译

Buildroot 里至少需要启用 Qt Base、Qt Network、Qt Declarative/Qt Quick、OpenGL ES/EGL、目标板 GPU/DRM 相关库、`rtl-sdr`/`librtlsdr`，以及 AM/FM 默认播放用的 ALSA 工具 `aplay`。不同 BSP 的选项名会有差异，核心是目标 sysroot 里必须能被 CMake 找到 `Qt5Quick`/`Qt5Network` 或 `Qt6Quick`/`Qt6Network`，并且能链接 `librtlsdr`。

复制工具链模板并设置 buildroot host 路径：

```sh
cp cmake/rk3566-buildroot-toolchain.cmake.example cmake/rk3566-buildroot-toolchain.cmake
export RK3566_BUILDROOT_HOST=/path/to/buildroot/output/host
cmake -S . -B build-rk3566 \
  -DCMAKE_TOOLCHAIN_FILE=cmake/rk3566-buildroot-toolchain.cmake \
  -DRTLSDR_PANEL_TARGET=rk3566 \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build-rk3566 -j
```

目标板运行时，单应用全屏通常使用 EGLFS：

```sh
export QT_QPA_PLATFORM=eglfs
export QT_QPA_EGLFS_FORCEVSYNC=1
/usr/bin/rtlsdr-panel
```

`deploy/rtlsdr-panel.env.example` 里给了运行环境变量示例。ADS-B、AM、FM、I/Q 录制、频谱瀑布图和 433/315 MHz 报文解码都是内置 RTL-SDR 后端。

## Raspberry Pi 编译

如果直接在 Raspberry Pi OS 上本机编译，安装 Qt Quick 开发包后执行：

```sh
cmake -S . -B build-rpi \
  -DRTLSDR_PANEL_TARGET=raspberrypi \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build-rpi -j
./build-rpi/rtlsdr-panel
```

如果 Raspberry Pi 也使用 buildroot，可用交叉编译模板。默认按 64 位 `aarch64-buildroot-linux-gnu` 处理：

```sh
cp cmake/raspberrypi-buildroot-toolchain.cmake.example cmake/raspberrypi-buildroot-toolchain.cmake
export RPI_BUILDROOT_HOST=/path/to/raspberrypi-buildroot/output/host
cmake -S . -B build-rpi \
  -DCMAKE_TOOLCHAIN_FILE=cmake/raspberrypi-buildroot-toolchain.cmake \
  -DRTLSDR_PANEL_TARGET=raspberrypi \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build-rpi -j
```

如果是 32 位 Raspberry Pi buildroot，按你的工具链三元组覆盖：

```sh
export RPI_TOOLCHAIN_TRIPLET=arm-buildroot-linux-gnueabihf
export RPI_SYSTEM_PROCESSOR=arm
```

无桌面环境全屏运行时可参考 `deploy/raspberrypi.env.example`，常见选择是 `QT_QPA_PLATFORM=eglfs`；如果系统已经运行 X11/Wayland，则使用系统默认平台插件即可。

## 后续接入建议

- ADS-B：当前已内置 DF17 呼号、高度、速度/航向和 CPR 经纬度解码；地图固定使用高德瓦片，后续可扩展离线瓦片、自建瓦片服务或更丰富的航迹分析。
- AM：当前已内置单声道包络检波、AGC 和 PCM 播放，后续可扩展 SSB、同步检波、静噪、预设频道和录音。
- FM：当前已内置 WFM/NFM 单声道解调和 PCM 播放，后续可扩展立体声、RDS、静噪、预设频道和录音。
- 频谱瀑布图：当前由 C++ 后端读取 RTL-SDR I/Q 数据并做 FFT，再把归一化频谱行送入 QML Canvas；数据量变大后可改为 `QQuickFramebufferObject` 或自定义 Scene Graph 节点。
- 433/315 MHz 报文：当前已内置 OOK/ASK 脉冲检测、常见 PPM 温湿度协议解析、EV1527/SC226x 风格 PWM 安防报文解析、HCS200/HCS300/HCS361 等汽车 RKE 被动识别、固定长度 RKE 窗口归一化和通用原始码输出，后续可按现场设备型号补 CRC、滚动码、门磁/水浸/气象站专用字段。
- 录制：当前已内置原始 `u8` I/Q 文件录制，后续可扩展定时停止、磁盘剩余空间预警、SigMF 元数据和录制回放。
