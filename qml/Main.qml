import QtQuick 2.12
import QtQuick.Window 2.12

Window {
    id: root
    width: 1280
    height: 720
    title: "RTL-SDR Panel"
    visibility: root.embeddedUi ? Window.FullScreen : Window.Windowed
    flags: root.embeddedUi ? (Qt.Window | Qt.FramelessWindowHint) : Qt.Window
    color: "#101316"
    property bool embeddedUi: panelEmbeddedUi
    property bool compactUi: width <= 900 || height <= 520
    property int pageMargin: compactUi ? 16 : 40
    property int footerHeight: compactUi ? 42 : 56
    property var activeFeature: null
    property var features: [
        {
            "id": "adsb",
            "title": "ADS-B 飞机识别",
            "subtitle": "1090 MHz 解码 / 航迹地图",
            "accent": "#4cc9f0",
            "icon": "plane"
        },
        {
            "id": "fm",
            "title": "FM 接收",
            "subtitle": "宽窄带调频解调",
            "accent": "#80ed99",
            "icon": "fm"
        },
        {
            "id": "waterfall",
            "title": "频谱瀑布图",
            "subtitle": "实时 FFT / 信号观察",
            "accent": "#f77f00",
            "icon": "waterfall"
        },
        {
            "id": "sensors",
            "title": "433/315 报文解码",
            "subtitle": "传感器 / 门磁 / 汽车钥匙",
            "accent": "#2dd4bf",
            "icon": "sensor"
        },
        {
            "id": "record",
            "title": "录制射频信号",
            "subtitle": "I/Q 基带采集与回放",
            "accent": "#ff5c8a",
            "icon": "record"
        },
        {
            "id": "scanner",
            "title": "宽带频谱扫描",
            "subtitle": "多跳扫描 / 热力图",
            "accent": "#a855f7",
            "icon": "waterfall"
        }
    ]

    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#15191d" }
            GradientStop { position: 1.0; color: "#0d1013" }
        }
    }

    Flickable {
        id: home
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: footer.top
        anchors.leftMargin: root.pageMargin
        anchors.rightMargin: root.pageMargin
        contentWidth: width
        contentHeight: grid.y + grid.height + 28
        clip: true
        visible: activeFeature === null

        Grid {
            id: grid
            x: root.compactUi ? 0 : 8
            y: root.compactUi ? 12 : 18
            width: parent.width - 16
            spacing: root.compactUi ? 12 : 18
            columns: Math.max(1, Math.floor(width / (root.compactUi ? 224 : 248)))

            Repeater {
                model: root.features

                AppTile {
                    width: Math.max(root.compactUi ? 190 : 210, Math.min(root.compactUi ? 246 : 260, (grid.width - grid.spacing * (grid.columns - 1)) / grid.columns))
                    height: 176
                    appId: modelData.id
                    title: modelData.title
                    subtitle: modelData.subtitle
                    accent: modelData.accent
                    iconKind: modelData.icon

                    onActivated: {
                        root.activeFeature = modelData
                    }
                }
            }
        }
    }

    FeaturePage {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: footer.top
        anchors.leftMargin: root.pageMargin
        anchors.rightMargin: root.pageMargin
        visible: root.activeFeature !== null
        feature: root.activeFeature

        onCloseRequested: root.activeFeature = null
        onStartRequested: {
            if (appId === "adsb") {
                if (adsbReceiver.running) {
                    adsbReceiver.stop()
                } else {
                    adsbReceiver.start()
                }
            } else if (appId === "fm") {
                if (fmReceiver.running) {
                    fmReceiver.stop()
                } else {
                    fmReceiver.start()
                }
            } else if (appId === "waterfall") {
                if (waterfallReceiver.running) {
                    waterfallReceiver.stop()
                } else {
                    waterfallReceiver.start()
                }
            } else if (appId === "sensors") {
                if (sensorReceiver.running) {
                    sensorReceiver.stop()
                } else {
                    sensorReceiver.start()
                }
            } else if (appId === "record") {
                if (recordReceiver.running) {
                    recordReceiver.stop()
                } else {
                    recordReceiver.start()
                }
            } else if (appId === "scanner") {
                if (scannerReceiver.running) {
                    scannerReceiver.stop()
                } else {
                    scannerReceiver.start()
                }
            } else {
                appLauncher.launch(appId)
            }
        }
    }

    Rectangle {
        id: footer
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: root.footerHeight
        color: "#0a0d10"

        Rectangle {
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            height: 1
            color: "#26313a"
        }

        Text {
            anchors.left: parent.left
            anchors.leftMargin: root.pageMargin + 8
            anchors.verticalCenter: parent.verticalCenter
            width: parent.width - (root.pageMargin + 8) * 2
            elide: Text.ElideRight
            text: root.activeFeature && root.activeFeature.id === "adsb" ? adsbReceiver.status
                  : root.activeFeature && root.activeFeature.id === "fm" ? fmReceiver.status
                  : root.activeFeature && root.activeFeature.id === "record" ? recordReceiver.status
                  : root.activeFeature && root.activeFeature.id === "sensors" ? sensorReceiver.status
                  : root.activeFeature && root.activeFeature.id === "waterfall" ? waterfallReceiver.status
                  : root.activeFeature && root.activeFeature.id === "scanner" ? scannerReceiver.status
                  : appLauncher.lastMessage
            color: "#c7d0d9"
            font.pixelSize: root.compactUi ? 12 : 14
        }
    }
}
