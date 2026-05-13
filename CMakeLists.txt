cmake_minimum_required(VERSION 3.16)

project(rtlsdr_panel
    VERSION 0.1.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(RTLSDR_PANEL_QT_MAJOR "auto" CACHE STRING "Qt major version: auto, 5, 6")
set_property(CACHE RTLSDR_PANEL_QT_MAJOR PROPERTY STRINGS auto 5 6)

set(RTLSDR_PANEL_TARGET "auto" CACHE STRING "Target platform: auto, desktop, rk3566, raspberrypi")
set_property(CACHE RTLSDR_PANEL_TARGET PROPERTY STRINGS auto desktop rk3566 raspberrypi)

if(NOT RTLSDR_PANEL_TARGET STREQUAL "auto"
        AND NOT RTLSDR_PANEL_TARGET STREQUAL "desktop"
        AND NOT RTLSDR_PANEL_TARGET STREQUAL "rk3566"
        AND NOT RTLSDR_PANEL_TARGET STREQUAL "raspberrypi")
    message(FATAL_ERROR "Unsupported RTLSDR_PANEL_TARGET: ${RTLSDR_PANEL_TARGET}. Use auto, desktop, rk3566, or raspberrypi.")
endif()

message(STATUS "RTL-SDR panel target: ${RTLSDR_PANEL_TARGET}")

if(RTLSDR_PANEL_QT_MAJOR STREQUAL "5")
    set(RTLSDR_PANEL_QT_CANDIDATES Qt5)
elseif(RTLSDR_PANEL_QT_MAJOR STREQUAL "6")
    set(RTLSDR_PANEL_QT_CANDIDATES Qt6)
elseif(RTLSDR_PANEL_QT_MAJOR STREQUAL "auto")
    set(RTLSDR_PANEL_QT_CANDIDATES Qt6 Qt5)
else()
    message(FATAL_ERROR "Unsupported RTLSDR_PANEL_QT_MAJOR: ${RTLSDR_PANEL_QT_MAJOR}. Use auto, 5, or 6.")
endif()

find_package(QT NAMES ${RTLSDR_PANEL_QT_CANDIDATES} REQUIRED COMPONENTS Core Gui Network Quick)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Gui Network Quick)
message(STATUS "RTL-SDR panel Qt version: Qt ${QT_VERSION_MAJOR}")

set(RTLSDR_ROOT "" CACHE PATH "Root directory of librtlsdr, useful on Windows when no CMake package or pkg-config file is available")
set(RTLSDR_RUNTIME_DLL "" CACHE FILEPATH "Path to rtlsdr.dll for Windows deployment")
set(LIBUSB_RUNTIME_DLL "" CACHE FILEPATH "Path to libusb-1.0.dll for Windows deployment")
option(RTLSDR_PANEL_WIN32_GUI "Build rtlsdr-panel as a Windows GUI executable without a console window" OFF)

set(_RTLSDR_HINTS)
foreach(_RTLSDR_ROOT_CANDIDATE IN ITEMS "${RTLSDR_ROOT}" "$ENV{RTLSDR_ROOT}" "$ENV{RTLSDR_DIR}")
    if(_RTLSDR_ROOT_CANDIDATE)
        list(APPEND _RTLSDR_HINTS "${_RTLSDR_ROOT_CANDIDATE}")
    endif()
endforeach()

find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(RTLSDR QUIET IMPORTED_TARGET librtlsdr)
endif()

set(RTLSDR_LINK_TARGET "")
if(TARGET PkgConfig::RTLSDR)
    set(RTLSDR_LINK_TARGET PkgConfig::RTLSDR)
else()
    find_package(rtlsdr CONFIG QUIET)
    foreach(_RTLSDR_TARGET_CANDIDATE IN ITEMS rtlsdr::rtlsdr RTLSDR::rtlsdr RTLSDR::RTLSDR rtlsdr)
        if(NOT RTLSDR_LINK_TARGET AND TARGET ${_RTLSDR_TARGET_CANDIDATE})
            set(RTLSDR_LINK_TARGET ${_RTLSDR_TARGET_CANDIDATE})
        endif()
    endforeach()
endif()

if(NOT RTLSDR_LINK_TARGET)
    find_path(RTLSDR_INCLUDE_DIR
        NAMES rtl-sdr.h
        HINTS ${_RTLSDR_HINTS}
        PATH_SUFFIXES include include/rtl-sdr
    )
    find_library(RTLSDR_LIBRARY
        NAMES rtlsdr librtlsdr
        HINTS ${_RTLSDR_HINTS}
        PATH_SUFFIXES lib lib64 bin
    )

    if(NOT RTLSDR_INCLUDE_DIR OR NOT RTLSDR_LIBRARY)
        message(FATAL_ERROR
            "Could not find librtlsdr. Install librtlsdr development files, "
            "or set RTLSDR_ROOT/RTLSDR_DIR to a directory containing include/rtl-sdr.h and lib/rtlsdr.")
    endif()

    add_library(rtlsdr_manual UNKNOWN IMPORTED)
    set_target_properties(rtlsdr_manual PROPERTIES
        IMPORTED_LOCATION "${RTLSDR_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${RTLSDR_INCLUDE_DIR}"
    )
    set(RTLSDR_LINK_TARGET rtlsdr_manual)
endif()
message(STATUS "RTL-SDR link target: ${RTLSDR_LINK_TARGET}")

set(RTLSDR_PANEL_RUNTIME_DLLS)
if(WIN32)
    if(NOT RTLSDR_RUNTIME_DLL)
        find_file(_RTLSDR_RUNTIME_DLL
            NAMES rtlsdr.dll librtlsdr.dll
            HINTS ${_RTLSDR_HINTS}
            PATH_SUFFIXES bin lib
        )
        if(_RTLSDR_RUNTIME_DLL)
            set(RTLSDR_RUNTIME_DLL "${_RTLSDR_RUNTIME_DLL}" CACHE FILEPATH "Path to rtlsdr.dll for Windows deployment" FORCE)
        endif()
    endif()

    if(NOT LIBUSB_RUNTIME_DLL)
        find_file(_LIBUSB_RUNTIME_DLL
            NAMES libusb-1.0.dll
            HINTS ${_RTLSDR_HINTS}
            PATH_SUFFIXES bin lib
        )
        if(_LIBUSB_RUNTIME_DLL)
            set(LIBUSB_RUNTIME_DLL "${_LIBUSB_RUNTIME_DLL}" CACHE FILEPATH "Path to libusb-1.0.dll for Windows deployment" FORCE)
        endif()
    endif()

    foreach(_RTLSDR_PANEL_DLL IN ITEMS "${RTLSDR_RUNTIME_DLL}" "${LIBUSB_RUNTIME_DLL}")
        if(_RTLSDR_PANEL_DLL)
            list(APPEND RTLSDR_PANEL_RUNTIME_DLLS "${_RTLSDR_PANEL_DLL}")
        endif()
    endforeach()
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
    qt_add_executable(rtlsdr-panel MANUAL_FINALIZATION ${PROJECT_SOURCES})
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

target_link_libraries(rtlsdr-panel PRIVATE ${RTLSDR_LINK_TARGET})

target_compile_definitions(rtlsdr-panel
    PRIVATE
        APP_VERSION="${PROJECT_VERSION}"
        RTLSDR_PANEL_TARGET="${RTLSDR_PANEL_TARGET}"
)

if(QT_VERSION_MAJOR EQUAL 6)
    set_target_properties(rtlsdr-panel PROPERTIES QT_QML_MODULE_NO_IMPORT_SCAN TRUE)
endif()

if(WIN32)
    set_target_properties(rtlsdr-panel PROPERTIES WIN32_EXECUTABLE "${RTLSDR_PANEL_WIN32_GUI}")
endif()

install(TARGETS rtlsdr-panel RUNTIME DESTINATION bin)
if(WIN32 AND RTLSDR_PANEL_RUNTIME_DLLS)
    install(FILES ${RTLSDR_PANEL_RUNTIME_DLLS} DESTINATION bin)
endif()

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(rtlsdr-panel)
endif()

if(WIN32)
    foreach(_RTLSDR_PANEL_DLL IN LISTS RTLSDR_PANEL_RUNTIME_DLLS)
        add_custom_command(TARGET rtlsdr-panel POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "${_RTLSDR_PANEL_DLL}"
                "$<TARGET_FILE_DIR:rtlsdr-panel>"
            COMMENT "Copying ${_RTLSDR_PANEL_DLL} next to rtlsdr-panel.exe"
            VERBATIM
        )
    endforeach()

    set(_QT_BIN_DIR "")
    if(TARGET Qt${QT_VERSION_MAJOR}::qmake)
        get_target_property(_QT_QMAKE_EXECUTABLE Qt${QT_VERSION_MAJOR}::qmake IMPORTED_LOCATION)
    endif()
    if(_QT_QMAKE_EXECUTABLE)
        get_filename_component(_QT_BIN_DIR "${_QT_QMAKE_EXECUTABLE}" DIRECTORY)
    endif()

    find_program(WINDEPLOYQT_EXECUTABLE
        NAMES windeployqt windeployqt.exe
        HINTS "${_QT_BIN_DIR}"
    )
    set(RTLSDR_PANEL_WINDEPLOYQT_MODE "--release" CACHE STRING "Build mode argument passed to windeployqt, usually --release or --debug")

    if(WINDEPLOYQT_EXECUTABLE)
        set(_RTLSDR_PANEL_DEPLOY_COMMANDS
            COMMAND "${WINDEPLOYQT_EXECUTABLE}"
                ${RTLSDR_PANEL_WINDEPLOYQT_MODE}
                --qmldir "${CMAKE_CURRENT_SOURCE_DIR}/qml"
                "$<TARGET_FILE:rtlsdr-panel>"
        )
        foreach(_RTLSDR_PANEL_DLL IN LISTS RTLSDR_PANEL_RUNTIME_DLLS)
            list(APPEND _RTLSDR_PANEL_DEPLOY_COMMANDS
                COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                    "${_RTLSDR_PANEL_DLL}"
                    "$<TARGET_FILE_DIR:rtlsdr-panel>"
            )
        endforeach()

        add_custom_target(deploy-windows
            ${_RTLSDR_PANEL_DEPLOY_COMMANDS}
            DEPENDS rtlsdr-panel
            COMMENT "Deploying Qt, QML, and RTL-SDR runtime files next to rtlsdr-panel.exe"
            VERBATIM
        )
    else()
        message(WARNING "windeployqt was not found. Add the Qt bin directory to PATH or run windeployqt manually after building.")
    endif()
endif()
