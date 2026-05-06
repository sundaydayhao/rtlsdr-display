https://www.rtl-sdr.com/big-list-rtl-sdr-supported-software/

这篇页面是RTL-SDR支持的软件大全，按**通用型、专用型、研究型、音频管道兼容型**四大类梳理了数百款适配软件，覆盖RTL-SDR从基础接收、信号解码到专业分析、科研开发的全场景应用，同时包含各系统（Windows/Linux/OSX/Android/iOS/树莓派）的适配方案，核心应用方向可归纳为以下8大板块，各板块均有对应的代表软件及功能特色：

### 一、通用宽带无线电接收
核心是让RTL-SDR实现常规宽频收音机的基础功能，支持多调制模式（AM/FM/WFM/LSB/USB/CW等）、频谱/瀑布图显示，是最基础的应用方向。
- 代表软件：SDR#（最主流，轻量且支持插件）、HDSDR（带音频滤波/陷波）、GQRX（Linux/Mac专属，操作简洁）、SDR++（跨平台开源）、SDRUno（原商业软件免费版）
- 特色功能：多VFO调谐、噪声抑制、频率管理、远程网络控制（如SDR-RADIO.COM V2/V3）

### 二、航空相关信号接收与解码
针对航空频段的各类信号解析，是RTL-SDR最热门的应用方向之一，覆盖飞机位置、航班信息、航空通信等全维度。
1. ADS-B/Mode S解码：dump1090（性能最优，带网页地图）、RTL1090、ADSB#，可将飞机ICAO、高度、速度等信息可视化；
2. 航空频段音频解码：RTLSDR-Airband（同时解8个航空信道，推流至LiveATC）；
3. 航空导航解码：vor-python-decoder/vortrack（VOR导航信标解码）；
4. 航空数据解码：rtl_acars_ng/Acarsdec（ACARS航空报文解码）、dumpvdl2/VDLM2DEC（VDL2航空数字链路解码）。

### 三、海事与船舶信号解码
聚焦海事频段，解析船舶位置、航行数据等AIS信号，支持地图可视化。
- 代表软件：AISdeco2（推荐款）、rtl_ais、PNAIS、Ships（Android端）、ShipPlotter（带海图可视化）；
- 配套工具：AISMon（AIS数据解码）+ OpenCPN（海事地图可视化）组合。

### 四、广播信号接收与解码
覆盖传统调频、数字广播的全类型接收，支持不同地区数字广播标准。
1. 调频/调幅广播：Chrome Radio Receiver（浏览器端FM）、Sodira（支持DRM/DRM+数字广播）；
2. 数字广播DAB/DAB+：Welle.io（界面专业）、SDR-J、DAB Player；
3. 美国高清广播：NRSC5 HD Radio Decoder（逆向解析美国HD Radio）；
4. RDS解码：Redsea、RDS Spy（高灵敏度广播数据解码）。

### 五、频谱分析与频率扫描
针对射频频谱的监测、扫描、可视化，支持宽频扫描、信号热图、频谱记录，适用于射频环境分析、信号定位。
- 宽频扫描：RTLSDR Scanner（跨平台）、RTL Panorama（扫描更快）、Spektrum（支持SWR/滤波器测量）；
- 频谱可视化：rtl_power（命令行宽频监测）、rtl_power_fftw（高效FFT处理）、Threejs-Spectrum（3D瀑布图）；
- 信号热图：rtl_heatmap、rtl-gopow（将扫描数据生成热图）；
- 特色工具：EMI_Mapper（结合摄像头生成电磁干扰热图）。

### 六、民用/工业传感器与设备信号解码
解析民用、工业级无线传感器及智能设备的射频信号，覆盖气象、家居、电力等场景。
1. 气象传感器：OregonWeather（欧西亚气象传感器）、SDRWeather（美国/加拿大气象广播，解EAS警报）；
2. 家居自动化：rtl_433（433MHz ISM频段传感器，温湿度/门磁等）、rtl_sdr_FS20_decoder（FS20家居协议）；
3. 电力/计量：rtlamr（900MHz智能电表）、GR-Elster（智能电表解码）、ec3k（电能记录仪解码）；
4. 其他设备：rtlmm（胰岛素泵数据包嗅探）、NRF24-BTLE Decode（鼠标/键盘蓝牙信号解码）。

### 七、专业射频分析与逆向工程
面向射频工程师、科研人员，支持数字信号解析、逆向工程、信号特征提取，是高阶应用方向。
- 信号逆向：Inspectrum/DSpectrumGUI（数字信号二进制提取）、Universal Radio Hacker（高级射频信号逆向）；
- 数字协议解码：Unitrunker/Trunk88（集群通信控制信道解码）、Telive/TETRA Listener（TETRA数字语音解码）、IMSI-Catcher（GSM手机IMSI号采集）；
- 干扰/漏洞检测：Salamandra（无线窃听器检测）、deinvert（语音倒频加密破解）；
- 特殊信号解析：TempestSDR（从显示器辐射信号还原视频）、Echoes（流星散射信号检测）。

### 八、卫星信号接收与解码
覆盖气象卫星、通信卫星、业余卫星等各类卫星信号的接收、解调与数据解析，需配合合适天线/上变频器。
1. 气象卫星：WxtoIMG/noaa-apt（NOAA气象卫星APT图像解码）、goestools/XRIT Decoder（GOES卫星HRIT/LRIT图像）、glrpt/meteor_demod（Meteor M2卫星LRPT图像）；
2. 通信卫星：gr-iridium/Iridium-Toolkit（铱星信号解码）、JAERO（海事卫星AERO信号解码）、Blockstream Satellite（比特币卫星区块链接收）；
3. 业余/科研卫星：SATNOGS（众包卫星地面站）、r2cloud（树莓派卫星解码基站）、Funcube Telemetry Dashboard（业余卫星遥测解码）；
4. 卫星导航：GNSS-SDR/GNSS-SDRLIB（GPS/北斗信号软解码）。

### 补充：研究开发与配套工具
1. 科研开发平台：GNU Radio（可视化DSP开发，RTL-SDR核心开发工具）、Redhawk（实时软件无线电开发）、MATLAB/ LabVIEW（专业仿真工具，需适配包）；
2. 硬件配套/校准：kalibrate-rtl（RTL-SDR频偏PPM校准）、RTL_HPSDR（对接HPSDR硬件）；
3. 数据共享/远程控制：rtl_tcp/SpyServer（IQ数据网络流）、OpenWebRX（网页端远程SDR）、PiAware（ADS-B数据上传FlightAware）；
4. 业余无线电配套：WSJT-X（FT8/WSPR弱信号解码）、Fldigi/multiPSK（业余数字模式解码）、SvxLink（中继台控制）。

此外，页面还包含**树莓派/嵌入式专用软件**（如PiAware、RaspberryApp）、**移动设备端软件**（Android：SDR Touch/Wavesink Plus；iOS：rtl_tcp SDR），并持续更新新增软件及功能升级，是RTL-SDR用户的核心参考手册。