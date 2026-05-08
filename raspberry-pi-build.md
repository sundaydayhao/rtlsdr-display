  1. Buildroot 环境：在 PC 上生成“树莓派系统 + 交叉工具链 + sysroot”。
  2. 交叉编译本项目：用 Buildroot 生成的 output/host 来编译 rtlsdr-panel。

  结论先说：树莓派不一定必须运行 Buildroot 系统，但如果你用这个项目里的 cmake/raspberrypi-buildroot-toolchain.cmake.example:1 交叉编译，最稳妥的是让树莓派运行同一个 Buildroot 生成的系统。否则库版本、Qt 插件、glibc ABI 很容易不匹配。

  推荐选择

  - 如果树莓派运行的是 Raspberry Pi OS：推荐直接在树莓派本机编译，最省事。
  - 如果你想在 PC 上交叉编译并可控部署：推荐 PC 上搭 Buildroot，并让树莓派也刷同一个 Buildroot 镜像。
  - 如果树莓派是 Raspberry Pi OS 但仍想 PC 交叉编译：可以做，但应使用 Raspberry Pi OS 的 sysroot，不建议用 Buildroot sysroot 混编。

  1. 安装 PC 依赖
  以 Ubuntu/Debian PC 为例：

  sudo apt update
  sudo apt install -y \
    git build-essential gcc g++ make cmake pkg-config \
    bc bison flex cpio rsync file wget curl unzip python3 perl \
    libncurses-dev libssl-dev

  2. 下载 Buildroot
  建议用单独目录，不要放进本项目里：

  mkdir -p ~/buildroot-work
  cd ~/buildroot-work

  git clone https://gitlab.com/buildroot.org/buildroot.git
  cd buildroot

  如果你想用稳定版，可以查看 tag：

  git tag -l | tail

  然后选一个稳定 tag，例如：

  git checkout 2026.02

  如果这个 tag 不存在，就用你本地 git tag -l 看到的最新稳定版。

  3. 选择树莓派 defconfig
  先看 Buildroot 支持哪些 Raspberry Pi 配置：

  ls configs/raspberrypi*_defconfig

  常见选择大概是：

  make O=~/buildroot-work/rpi-output raspberrypi4_64_defconfig

  如果你是 Pi 3 64 位，可能是：

  make O=~/buildroot-work/rpi-output raspberrypi3_64_defconfig

  如果你是 Pi 5，就先用这个确认名字：

  ls configs/raspberrypi5*_defconfig

  然后执行对应的：

  make O=~/buildroot-work/rpi-output 对应的_defconfig

  这里 O=~/buildroot-work/rpi-output 表示把编译输出放到独立目录。后面本项目需要的 RPI_BUILDROOT_HOST 就是：

  ~/buildroot-work/rpi-output/host

  4. 配置 Buildroot 软件包
  进入菜单：

  make O=~/buildroot-work/rpi-output menuconfig

  至少需要关注这些：

  Toolchain  --->
    [*] Enable C++ support

  Target packages  --->
    Libraries  --->
      Hardware handling  --->
        [*] libusb
        [*] rtl-sdr

    Audio and video applications  --->
        [*] alsa-utils

    Graphic libraries and applications  --->
        Qt5 / Qt6 相关包

  本项目需要 Qt 模块：

  Qt Core
  Qt Gui
  Qt Network
  Qt Quick

  也就是说 Buildroot 里至少要有：

  qtbase
  qtdeclarative / qtquick

  如果你不确定 Buildroot 里包在哪，可以在 menuconfig 里按 / 搜索：

  qt5base
  qt5declarative
  qt6base
  qt6declarative
  rtl-sdr
  alsa-utils

  对于这个项目，我建议 Buildroot 里优先用 Qt5，因为很多 Buildroot 配置下 Qt5 更成熟。那交叉编译项目时加：

  -DRTLSDR_PANEL_QT_MAJOR=5

  5. 编译 Buildroot
  这一步会很久，第一次可能几十分钟到数小时：

  make O=~/buildroot-work/rpi-output -j$(nproc)

  完成后关键目录是：

  ~/buildroot-work/rpi-output/host
  ~/buildroot-work/rpi-output/images
  ~/buildroot-work/rpi-output/target

  其中：

  - host：PC 上用的交叉编译器、CMake、pkg-config、sysroot。
  - images：给树莓派刷机用的镜像。
  - target：目标 rootfs 内容。

  6. 用 Buildroot 交叉编译本项目
  回到本项目：

  cd /home/hao/projects/rtlsdr

  export RPI_BUILDROOT_HOST=~/buildroot-work/rpi-output/host

  rm -rf build-rpi
  cmake -S . -B build-rpi \
    -DCMAKE_TOOLCHAIN_FILE=cmake/raspberrypi-buildroot-toolchain.cmake.example \
    -DCMAKE_BUILD_TYPE=Release \
    -DRTLSDR_PANEL_TARGET=raspberrypi \
    -DRTLSDR_PANEL_QT_MAJOR=5

  cmake --build build-rpi -j$(nproc)

  产物是：

  build-rpi/rtlsdr-panel

  项目 README 里的交叉编译入口也在 README.md:176。

  7. 树莓派是否必须是 Buildroot 系统？
  不必须，但分情况：

  情况 A：树莓派也刷 Buildroot 镜像
  这是最推荐的交叉编译方式。

  你的程序链接的是：

  ~/buildroot-work/rpi-output/host/.../sysroot

  树莓派运行的也是同一个 Buildroot 生成的 rootfs，所以 Qt、glibc、librtlsdr、libusb、ALSA 都匹配。

  把镜像烧录到 SD 卡：

  ls ~/buildroot-work/rpi-output/images

  通常会有类似：

  sdcard.img

  烧录，例如：

  sudo dd if=~/buildroot-work/rpi-output/images/sdcard.img of=/dev/sdX bs=4M status=progress conv=fsync

  注意把 /dev/sdX 换成你的 SD 卡设备，别写错盘。

  情况 B：树莓派运行 Raspberry Pi OS
  这时不建议用 Buildroot 工具链编译出来的程序直接放过去跑。

  可能遇到：

  GLIBC_x.xx not found
  libQt5Core.so.5 not found
  Qt platform plugin "eglfs"/"xcb" not found
  librtlsdr.so not found

  如果目标系统是 Raspberry Pi OS，推荐：

  # 在树莓派本机
  sudo apt install -y build-essential cmake pkg-config \
    qtbase5-dev qtdeclarative5-dev \
    qml-module-qtquick2 qml-module-qtquick-window2 \
    librtlsdr-dev libusb-1.0-0-dev

  cd ~/rtlsdr
  rm -rf build
  cmake -S . -B build \
    -DCMAKE_BUILD_TYPE=Release \
    -DRTLSDR_PANEL_TARGET=raspberrypi \
    -DRTLSDR_PANEL_QT_MAJOR=5
  cmake --build build -j$(nproc)

  这是最省心的 Raspberry Pi OS 方案。

  情况 C：PC 交叉编译给 Raspberry Pi OS
  也可以，但要用 Raspberry Pi OS 的 sysroot，而不是 Buildroot sysroot。大致需要：

  - PC 安装 aarch64-linux-gnu-g++
  - 从树莓派同步 /lib、/usr/lib、/usr/include
  - 同步 Qt dev 文件和 pkg-config 文件
  - 修正 sysroot 里的绝对软链接
  - 自己写 Raspberry Pi OS 专用 CMake toolchain file

  这个路线比 Buildroot 或本机编译麻烦很多。

  我建议你现在选这个路线
  如果你只是想让现在这台 Raspberry Pi 上的程序跑起来，并且它已经是 Raspberry Pi OS：

  不要搭 Buildroot，直接树莓派本机编译。

  如果你想做一个可复制的嵌入式镜像、开机自启、固定依赖版本：

  PC 上 Buildroot + 树莓派刷同一个 Buildroot 镜像。

  你告诉我你的树莓派型号是 Pi 3 / Pi 4 / Pi 5，以及系统是 32 位还是 64 位，我可以直接给你一份对应的 Buildroot defconfig 配置清单。