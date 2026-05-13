  | 模块 | 建议配置 |
  |---|---|
  | UHD: USRP Source | Device Address = type=usrp1（也可以先留空，再用 uhd_find_devices --args="type=usrp1" 确认）；Sample Rate = 2e6；Center Freq = freq；Gain = gain，先从 20~40 dB 试；Antenna = RX；Wire Format = sc16 默认即可；Bandwidth = 0 |
  | Rational Resampler | Interpolation = 12，Decimation = 125，把 2 MHz 变成 192 kHz |
  | WBFM Receive | Channel Rate = 192e3；Audio Decimation = 4；Deviation = 75e3；Tau = 75e-6（美国）或 50e-6（欧洲/部分地区）；Audio Pass/Stop 先用默认；Gain = 1 起步 |
  | Audio Sink | Sample Rate = 48000 |
  | 可选 | QT GUI Range 控 freq 和 gain；QT GUI Frequency Sink 的 Center Frequency = freq、Bandwidth = samp_rate；如果想调音量，再在 WBFM Receive 后加 Multiply Const |
