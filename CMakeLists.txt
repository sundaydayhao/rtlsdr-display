cmake_minimum_required(VERSION 3.16)

project(rtlsdr_panel
    VERSION 0.1.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(RTLSDR_PANEL_TARGET "auto" CACHE STRING "Target platform: auto, desktop, rk3566, raspberrypi")
set_property(CACHE RTLSDR_PANEL_TARGET PROPERTY STRINGS auto desktop rk3566 raspberrypi)

set(RTLSDR_PANEL_TARGET_LABEL "")
if(RTLSDR_PANEL_TARGET STREQUAL "desktop")
    set(RTLSDR_PANEL_TARGET_LABEL "Desktop Linux")
elseif(RTLSDR_PANEL_TARGET STREQUAL "rk3566")
    set(RTLSDR_PANEL_TARGET_LABEL "RK3566 / Buildroot")
elseif(RTLSDR_PANEL_TARGET STREQUAL "raspberrypi")
    set(RTLSDR_PANEL_TARGET_LABEL "Raspberry Pi")
elseif(NOT RTLSDR_PANEL_TARGET STREQUAL "auto")
    message(FATAL_ERROR "Unsupported RTLSDR_PANEL_TARGET: ${RTLSDR_PANEL_TARGET}. Use auto, desktop, rk3566, or raspberrypi.")
endif()

message(STATUS "RTL-SDR panel target: ${RTLSDR_PANEL_TARGET}")

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Gui Network Quick)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Gui Network Quick)
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(RTLSDR QUIET IMPORTED_TARGET librtlsdr)
endif()
if(NOT TARGET PkgConfig::RTLSDR)
    find_package(rtlsdr CONFIG REQUIRED)
endif()

set(PROJECT_SOURCES
    src/main.cpp
    src/AppLauncher.cpp
    src/AppLauncher.h
    src/adsb/AdsbAircraftModel.cpp
    src/adsb/AdsbAircraftModel.h
    src/adsb/AdsbDecoder.cpp
    src/adsb/AdsbDecoder.h
    src/adsb/AdsbReceiver.cpp
    src/adsb/AdsbReceiver.h
    src/fm/FmReceiver.cpp
    src/fm/FmReceiver.h
    src/record/RecordReceiver.cpp
    src/record/RecordReceiver.h
    src/sensors/SensorDecoder.cpp
    src/sensors/SensorDecoder.h
    src/sensors/SensorEventModel.cpp
    src/sensors/SensorEventModel.h
    src/sensors/SensorReceiver.cpp
    src/sensors/SensorReceiver.h
    src/sensors/TpmsDecoder.cpp
    src/sensors/TpmsDecoder.h
    src/sensors/CarKeyDecoder.h
    src/sensors/UtilityMeterDecoder.cpp
    src/sensors/UtilityMeterDecoder.h
    src/sensors/SceneDecoder.h
    src/waterfall/WaterfallReceiver.cpp
    src/waterfall/WaterfallReceiver.h
    src/scanner/ScannerReceiver.cpp
    src/scanner/ScannerReceiver.h
    resources/qml.qrc
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_add_executable(rtlsdr-panel ${PROJECT_SOURCES})
else()
    add_executable(rtlsdr-panel ${PROJECT_SOURCES})
endif()

target_link_libraries(rtlsdr-panel
    PRIVATE
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Gui
        Qt${QT_VERSION_MAJOR}::Network
        Qt${QT_VERSION_MAJOR}::Quick
)

if(TARGET PkgConfig::RTLSDR)
    target_link_libraries(rtlsdr-panel PRIVATE PkgConfig::RTLSDR)
else()
    target_link_libraries(rtlsdr-panel PRIVATE rtlsdr::rtlsdr)
endif()

target_compile_definitions(rtlsdr-panel
    PRIVATE
        APP_VERSION="${PROJECT_VERSION}"
        RTLSDR_PANEL_TARGET="${RTLSDR_PANEL_TARGET}"
)

if(RTLSDR_PANEL_TARGET_LABEL)
    target_compile_definitions(rtlsdr-panel
        PRIVATE
            RTLSDR_PANEL_TARGET_LABEL="${RTLSDR_PANEL_TARGET_LABEL}"
    )
endif()

install(TARGETS rtlsdr-panel RUNTIME DESTINATION bin)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(rtlsdr-panel)
endif()
