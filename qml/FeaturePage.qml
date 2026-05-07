import QtQuick 2.12

Item {
    id: root

    property var feature: null
    property string appId: feature ? feature.id : ""
    property string title: feature ? feature.title : ""
    property string subtitle: feature ? feature.subtitle : ""
    property string iconKind: feature ? feature.icon : ""
    property color accent: feature ? feature.accent : "#4cc9f0"
    property bool adsbShowTracks: true

    signal closeRequested()
    signal startRequested(string appId)

    function mhzText(hz, decimals) {
        return (hz / 1000000).toFixed(decimals)
    }

    function sampleRateText(hz) {
        return (hz / 1000000).toFixed(3) + " MSPS"
    }

    function audioRateText(hz) {
        return (hz / 1000).toFixed(1) + " kHz"
    }

    function bytesText(bytes) {
        var value = Number(bytes)
        if (value < 1024) {
            return value.toFixed(0) + " B"
        }
        value = value / 1024
        if (value < 1024) {
            return value.toFixed(1) + " KiB"
        }
        value = value / 1024
        if (value < 1024) {
            return value.toFixed(1) + " MiB"
        }
        return (value / 1024).toFixed(2) + " GiB"
    }

    function metersText(feet) {
        return Math.round(feet * 0.3048) + " m"
    }

    function kmhText(knots) {
        return Math.round(knots * 1.852) + " km/h"
    }

    function durationText(seconds) {
        var total = Math.max(0, Math.floor(seconds))
        var minutes = Math.floor(total / 60)
        var rest = total % 60
        return minutes + ":" + (rest < 10 ? "0" + rest : rest)
    }

    function commitWaterfallFrequency() {
        var mhz = parseFloat(waterfallFrequencyInput.text)
        if (isNaN(mhz)) {
            waterfallFrequencyInput.text = mhzText(waterfallReceiver.centerFrequencyHz, 3)
            return
        }
        waterfallReceiver.tuneMHz(mhz)
        waterfallFrequencyInput.text = mhzText(waterfallReceiver.centerFrequencyHz, 3)
    }

    function commitSensorFrequency() {
        var mhz = parseFloat(sensorFrequencyInput.text)
        if (isNaN(mhz)) {
            sensorFrequencyInput.text = mhzText(sensorReceiver.centerFrequencyHz, 3)
            return
        }
        sensorReceiver.tuneMHz(mhz)
        sensorFrequencyInput.text = mhzText(sensorReceiver.centerFrequencyHz, 3)
    }

    function commitFmFrequency() {
        var mhz = parseFloat(fmFrequencyInput.text)
        if (isNaN(mhz)) {
            fmFrequencyInput.text = mhzText(fmReceiver.centerFrequencyHz, 3)
            return
        }
        fmReceiver.tuneMHz(mhz)
        fmFrequencyInput.text = mhzText(fmReceiver.centerFrequencyHz, 3)
    }

    function commitAmFrequency() {
        var mhz = parseFloat(amFrequencyInput.text)
        if (isNaN(mhz)) {
            amFrequencyInput.text = mhzText(amReceiver.centerFrequencyHz, 3)
            return
        }
        amReceiver.tuneMHz(mhz)
        amFrequencyInput.text = mhzText(amReceiver.centerFrequencyHz, 3)
    }

    function commitRecordFrequency() {
        var mhz = parseFloat(recordFrequencyInput.text)
        if (isNaN(mhz)) {
            recordFrequencyInput.text = mhzText(recordReceiver.centerFrequencyHz, 3)
            return
        }
        recordReceiver.tuneMHz(mhz)
        recordFrequencyInput.text = mhzText(recordReceiver.centerFrequencyHz, 3)
    }

    function commitRecordSampleRate() {
        var msps = parseFloat(recordSampleRateInput.text)
        if (isNaN(msps)) {
            recordSampleRateInput.text = (recordReceiver.sampleRateHz / 1000000).toFixed(3)
            return
        }
        recordReceiver.setSampleRateHz(msps * 1000000)
        recordSampleRateInput.text = (recordReceiver.sampleRateHz / 1000000).toFixed(3)
    }

    function commitRecordOutputPath() {
        recordReceiver.setOutputFilePath(recordPathInput.text)
        recordPathInput.text = recordReceiver.outputFilePath
    }

    function commitScannerRange() {
        var startMHz = parseFloat(scannerStartInput.text)
        var stopMHz  = parseFloat(scannerStopInput.text)
        if (isNaN(startMHz) || isNaN(stopMHz) || startMHz >= stopMHz) {
            scannerStartInput.text = mhzText(scannerReceiver.startFrequencyHz, 1)
            scannerStopInput.text  = mhzText(scannerReceiver.stopFrequencyHz,  1)
            return
        }
        scannerReceiver.setStartFrequencyHz(startMHz * 1000000)
        scannerReceiver.setStopFrequencyHz(stopMHz  * 1000000)
        scannerStartInput.text = mhzText(scannerReceiver.startFrequencyHz, 1)
        scannerStopInput.text  = mhzText(scannerReceiver.stopFrequencyHz,  1)
    }

    function focusAircraft(aircraft) {
        if (visualLoader.item && visualLoader.item.focusAircraft) {
            visualLoader.item.focusAircraft(aircraft)
        }
    }

    Connections {
        target: recordReceiver
        function onOutputFilePathChanged() {
            if (root.appId === "record") {
                recordPathInput.text = recordReceiver.outputFilePath
            }
        }
    }

    Rectangle {
        anchors.fill: parent
        anchors.topMargin: 12
        anchors.bottomMargin: 18
        radius: 8
        color: "#151a1f"
        border.color: "#29343d"
    }

    Rectangle {
        id: backButton
        anchors.left: parent.left
        anchors.leftMargin: 18
        anchors.top: parent.top
        anchors.topMargin: 30
        width: 44
        height: 36
        radius: 8
        color: backMouse.pressed ? "#29323a" : backMouse.containsMouse ? "#222a31" : "#101419"
        border.color: "#313c46"

        Canvas {
            anchors.centerIn: parent
            width: 18
            height: 18
            onPaint: {
                var ctx = getContext("2d")
                ctx.clearRect(0, 0, width, height)
                ctx.strokeStyle = "#edf3f8"
                ctx.lineWidth = 2.3
                ctx.lineCap = "round"
                ctx.lineJoin = "round"
                ctx.beginPath()
                ctx.moveTo(11, 3)
                ctx.lineTo(5, 9)
                ctx.lineTo(11, 15)
                ctx.stroke()
            }
        }

        MouseArea {
            id: backMouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: root.closeRequested()
        }
    }

    Rectangle {
        id: iconPlate
        anchors.left: backButton.right
        anchors.leftMargin: 16
        anchors.verticalCenter: backButton.verticalCenter
        width: 44
        height: 44
        radius: 8
        color: Qt.rgba(root.accent.r, root.accent.g, root.accent.b, 0.16)
        border.color: Qt.rgba(root.accent.r, root.accent.g, root.accent.b, 0.44)

        IconGlyph {
            anchors.centerIn: parent
            width: 30
            height: 30
            kind: root.iconKind
            accent: root.accent
        }
    }

    Text {
        id: titleLabel
        anchors.left: iconPlate.right
        anchors.leftMargin: 14
        anchors.right: launchButton.left
        anchors.rightMargin: 18
        anchors.top: parent.top
        anchors.topMargin: 28
        text: root.title
        color: "#f6f8fb"
        font.pixelSize: 24
        font.bold: true
        elide: Text.ElideRight
    }

    Text {
        anchors.left: titleLabel.left
        anchors.right: titleLabel.right
        anchors.top: titleLabel.bottom
        anchors.topMargin: 5
        text: root.subtitle
        color: "#9baab7"
        font.pixelSize: 13
        elide: Text.ElideRight
    }

    Rectangle {
        id: launchButton
        anchors.right: parent.right
        anchors.rightMargin: 22
        anchors.verticalCenter: backButton.verticalCenter
        width: 112
        height: 40
        radius: 8
        color: launchMouse.pressed ? Qt.darker(root.accent, 1.35) : launchMouse.containsMouse ? Qt.lighter(root.accent, 1.08) : root.accent

        Text {
            anchors.centerIn: parent
            text: (root.appId === "adsb" && adsbReceiver.running)
                  || (root.appId === "am" && amReceiver.running)
                  || (root.appId === "fm" && fmReceiver.running)
                  || (root.appId === "record" && recordReceiver.running)
                  || (root.appId === "sensors" && sensorReceiver.running)
                  || (root.appId === "waterfall" && waterfallReceiver.running)
                  || (root.appId === "scanner" && scannerReceiver.running) ? "停止" : "启动"
            color: "#071014"
            font.pixelSize: 15
            font.bold: true
        }

        MouseArea {
            id: launchMouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: root.startRequested(root.appId)
        }
    }

    Item {
        id: content
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.leftMargin: 22
        anchors.rightMargin: 22
        anchors.topMargin: 92
        anchors.bottomMargin: 34

        Loader {
            id: visualLoader
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            width: Math.max(420, parent.width - sidePanel.width - 22)
            sourceComponent: root.appId === "adsb" ? radarComponent
                             : root.appId === "am" ? amComponent
                             : root.appId === "fm" ? fmComponent
                             : root.appId === "record" ? recordComponent
                             : root.appId === "sensors" ? sensorComponent
                             : root.appId === "waterfall" ? waterfallComponent
                             : root.appId === "scanner" ? scannerComponent
                             : receiverComponent
        }

        Rectangle {
            id: sidePanel
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            width: Math.min(360, parent.width * 0.32)
            radius: 8
            color: "#101419"
            border.color: "#2b3640"

            Text {
                id: panelTitle
                anchors.left: parent.left
                anchors.leftMargin: 18
                anchors.right: parent.right
                anchors.rightMargin: 18
                anchors.top: parent.top
                anchors.topMargin: 18
                text: root.appId === "adsb" ? "ADS-B 接收"
                      : root.appId === "am" ? "AM 接收"
                      : root.appId === "fm" ? "FM 接收"
                      : root.appId === "record" ? "I/Q 录制"
                      : root.appId === "sensors" ? "无线报文解码"
                      : root.appId === "waterfall" ? "瀑布图接收"
                      : root.appId === "scanner" ? "宽带扫描"
                      : "运行配置"
                color: "#f3f6f8"
                font.pixelSize: 18
                font.bold: true
            }

            Text {
                id: commandLabel
                anchors.left: panelTitle.left
                anchors.right: panelTitle.right
                anchors.top: panelTitle.bottom
                anchors.topMargin: 18
                text: root.appId === "record" ? "录制状态"
                      : root.appId === "adsb" || root.appId === "am" || root.appId === "fm"
                        || root.appId === "sensors" || root.appId === "waterfall"
                        || root.appId === "scanner" ? "接收状态" : "启动命令"
                color: "#8d9daa"
                font.pixelSize: 12
            }

            Rectangle {
                id: commandBox
                anchors.left: panelTitle.left
                anchors.right: panelTitle.right
                anchors.top: commandLabel.bottom
                anchors.topMargin: 8
                height: 78
                radius: 8
                color: "#0b0f13"
                border.color: "#27313a"

                Text {
                    anchors.fill: parent
                    anchors.margins: 12
                    text: {
                        if (root.appId === "adsb") {
                            return adsbReceiver.status
                        }
                        if (root.appId === "am") {
                            return amReceiver.status
                        }
                        if (root.appId === "fm") {
                            return fmReceiver.status
                        }
                        if (root.appId === "record") {
                            return recordReceiver.status
                        }
                        if (root.appId === "sensors") {
                            return sensorReceiver.status
                        }
                        if (root.appId === "waterfall") {
                            return waterfallReceiver.status
                        }
                        if (root.appId === "scanner") {
                            return scannerReceiver.status
                        }
                        var cmd = appLauncher.commandFor(root.appId)
                        return cmd.length > 0 ? cmd : "未配置"
                    }
                    color: root.appId === "adsb" || root.appId === "am" || root.appId === "fm"
                           || root.appId === "record" || root.appId === "sensors"
                           || root.appId === "waterfall" || root.appId === "scanner"
                           || appLauncher.commandFor(root.appId).length > 0 ? "#d8e2ea" : "#788895"
                    font.pixelSize: 13
                    wrapMode: Text.Wrap
                    elide: Text.ElideRight
                }
            }

            Column {
                id: metricsColumn
                anchors.left: panelTitle.left
                anchors.right: panelTitle.right
                anchors.top: commandBox.bottom
                anchors.topMargin: 20
                spacing: root.appId === "am" || root.appId === "fm" || root.appId === "record" ? 8 : 12

                StatusPill {
                    width: parent.width
                    label: root.appId === "adsb" ? (adsbReceiver.running ? "RTL-SDR：接收中" : "RTL-SDR：停止")
                           : root.appId === "am" ? (amReceiver.running ? "RTL-SDR：播放中" : "RTL-SDR：停止")
                           : root.appId === "fm" ? (fmReceiver.running ? "RTL-SDR：播放中" : "RTL-SDR：停止")
                           : root.appId === "record" ? (recordReceiver.running ? "I/Q：录制中" : "I/Q：停止")
                           : root.appId === "sensors" ? (sensorReceiver.running ? "RTL-SDR：解码中" : "RTL-SDR：停止")
                           : root.appId === "waterfall" ? (waterfallReceiver.running ? "RTL-SDR：接收中" : "RTL-SDR：停止")
                           : root.appId === "scanner" ? (scannerReceiver.running ? "RTL-SDR：扫描中" : "RTL-SDR：停止")
                           : "解调：SDR 后端"
                    accent: root.accent
                }

                StatusPill {
                    width: parent.width
                    visible: root.appId !== "fm"
                    label: root.appId === "fm" ? "模式：WFM / NFM"
                           : root.appId === "am" ? "模式：AM"
                           : root.appId === "adsb" ? ("设备：" + adsbReceiver.deviceCount + " 个")
                           : root.appId === "record" ? ("设备：" + recordReceiver.deviceCount + " 个")
                           : root.appId === "sensors" ? ("设备：" + sensorReceiver.deviceCount + " 个")
                           : root.appId === "waterfall" ? ("设备：" + waterfallReceiver.deviceCount + " 个")
                           : root.appId === "scanner" ? ("设备：" + scannerReceiver.deviceCount + " 个")
                           : "平台：本机 / RK3566"
                    accent: "#80ed99"
                }

                Column {
                    width: parent.width
                    visible: root.appId === "am"
                    spacing: 6

                    Text {
                        width: parent.width
                        visible: false
                        text: "频率 MHz"
                        color: "#8d9daa"
                        font.pixelSize: 12
                    }

                    Rectangle {
                        width: parent.width
                        height: 40
                        radius: 8
                        color: "#0b0f13"
                        border.color: amFrequencyInput.activeFocus ? root.accent : "#27313a"

                        TextInput {
                            id: amFrequencyInput
                            anchors.left: parent.left
                            anchors.leftMargin: 12
                            anchors.right: amUnitLabel.left
                            anchors.rightMargin: 8
                            anchors.verticalCenter: parent.verticalCenter
                            text: root.mhzText(amReceiver.centerFrequencyHz, 3)
                            color: "#eef6fb"
                            selectedTextColor: "#071014"
                            selectionColor: root.accent
                            font.pixelSize: 18
                            font.bold: true
                            selectByMouse: true
                            inputMethodHints: Qt.ImhFormattedNumbersOnly
                            onAccepted: root.commitAmFrequency()
                        }

                        Text {
                            id: amUnitLabel
                            anchors.right: parent.right
                            anchors.rightMargin: 12
                            anchors.verticalCenter: parent.verticalCenter
                            text: "MHz"
                            color: "#8d9daa"
                            font.pixelSize: 12
                        }
                    }

                    Row {
                        id: amStepRow
                        width: parent.width
                        height: 30
                        spacing: 8

                        Repeater {
                            model: [
                                { "label": "-0.1", "step": -0.1 },
                                { "label": "-0.01", "step": -0.01 },
                                { "label": "+0.01", "step": 0.01 },
                                { "label": "+0.1", "step": 0.1 }
                            ]

                            Rectangle {
                                width: (amStepRow.width - amStepRow.spacing * 3) / 4
                                height: amStepRow.height
                                radius: 8
                                color: amStepMouse.pressed ? "#29323a" : amStepMouse.containsMouse ? "#222a31" : "#101419"
                                border.color: "#313c46"

                                Text {
                                    anchors.centerIn: parent
                                    text: modelData.label
                                    color: "#d8e2ea"
                                    font.pixelSize: 13
                                    font.bold: true
                                }

                                MouseArea {
                                    id: amStepMouse
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: {
                                        amReceiver.stepMHz(modelData.step)
                                        amFrequencyInput.text = root.mhzText(amReceiver.centerFrequencyHz, 3)
                                    }
                                }
                            }
                        }
                    }

                    Rectangle {
                        width: parent.width
                        height: 32
                        radius: 8
                        color: amTuneMouse.pressed ? Qt.darker(root.accent, 1.35) : amTuneMouse.containsMouse ? Qt.lighter(root.accent, 1.08) : root.accent

                        Text {
                            anchors.centerIn: parent
                            text: "调谐"
                            color: "#071014"
                            font.pixelSize: 14
                            font.bold: true
                        }

                        MouseArea {
                            id: amTuneMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: root.commitAmFrequency()
                        }
                    }

                    Text {
                        width: parent.width
                        visible: false
                        text: root.mhzText(amReceiver.minFrequencyHz, 3)
                              + " - " + root.mhzText(amReceiver.maxFrequencyHz, 3) + " MHz"
                        color: "#788895"
                        font.pixelSize: 12
                        elide: Text.ElideRight
                    }

                    Text {
                        width: parent.width
                        visible: false
                        text: "音量"
                        color: "#8d9daa"
                        font.pixelSize: 12
                    }

                    Rectangle {
                        id: amVolumeTrack
                        width: parent.width
                        height: 32
                        radius: 8
                        color: "#0b0f13"
                        border.color: "#27313a"

                        Rectangle {
                            anchors.left: parent.left
                            anchors.top: parent.top
                            anchors.bottom: parent.bottom
                            anchors.margins: 3
                            width: Math.max(8, (parent.width - 6) * Math.min(1.0, amReceiver.volume / 1.5))
                            radius: 6
                            color: root.accent
                            opacity: 0.86
                        }

                        Text {
                            anchors.centerIn: parent
                            text: Math.round(amReceiver.volume * 100) + "%"
                            color: "#e6edf3"
                            font.pixelSize: 13
                            font.bold: true
                        }

                        MouseArea {
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor

                            function applyVolume(x) {
                                amReceiver.setVolume(Math.max(0, Math.min(1.5, x / width * 1.5)))
                            }

                            onClicked: applyVolume(mouse.x)
                            onPositionChanged: {
                                if (pressed) {
                                    applyVolume(mouse.x)
                                }
                            }
                        }
                    }
                }

                Column {
                    width: parent.width
                    visible: root.appId === "fm"
                    spacing: 6

                    Text {
                        width: parent.width
                        visible: false
                        text: "频率 MHz"
                        color: "#8d9daa"
                        font.pixelSize: 12
                    }

                    Rectangle {
                        width: parent.width
                        height: 40
                        radius: 8
                        color: "#0b0f13"
                        border.color: fmFrequencyInput.activeFocus ? root.accent : "#27313a"

                        TextInput {
                            id: fmFrequencyInput
                            anchors.left: parent.left
                            anchors.leftMargin: 12
                            anchors.right: fmUnitLabel.left
                            anchors.rightMargin: 8
                            anchors.verticalCenter: parent.verticalCenter
                            text: root.mhzText(fmReceiver.centerFrequencyHz, 3)
                            color: "#eef6fb"
                            selectedTextColor: "#071014"
                            selectionColor: root.accent
                            font.pixelSize: 18
                            font.bold: true
                            selectByMouse: true
                            inputMethodHints: Qt.ImhFormattedNumbersOnly
                            onAccepted: root.commitFmFrequency()
                        }

                        Text {
                            id: fmUnitLabel
                            anchors.right: parent.right
                            anchors.rightMargin: 12
                            anchors.verticalCenter: parent.verticalCenter
                            text: "MHz"
                            color: "#8d9daa"
                            font.pixelSize: 12
                        }
                    }

                    Row {
                        id: fmStepRow
                        width: parent.width
                        height: 30
                        spacing: 8

                        Repeater {
                            model: [
                                { "label": "-1", "step": -1 },
                                { "label": "-0.1", "step": -0.1 },
                                { "label": "+0.1", "step": 0.1 },
                                { "label": "+1", "step": 1 }
                            ]

                            Rectangle {
                                width: (fmStepRow.width - fmStepRow.spacing * 3) / 4
                                height: fmStepRow.height
                                radius: 8
                                color: fmStepMouse.pressed ? "#29323a" : fmStepMouse.containsMouse ? "#222a31" : "#101419"
                                border.color: "#313c46"

                                Text {
                                    anchors.centerIn: parent
                                    text: modelData.label
                                    color: "#d8e2ea"
                                    font.pixelSize: 13
                                    font.bold: true
                                }

                                MouseArea {
                                    id: fmStepMouse
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: {
                                        fmReceiver.stepMHz(modelData.step)
                                        fmFrequencyInput.text = root.mhzText(fmReceiver.centerFrequencyHz, 3)
                                    }
                                }
                            }
                        }
                    }

                    Rectangle {
                        width: parent.width
                        height: 32
                        radius: 8
                        color: fmTuneMouse.pressed ? Qt.darker(root.accent, 1.35) : fmTuneMouse.containsMouse ? Qt.lighter(root.accent, 1.08) : root.accent

                        Text {
                            anchors.centerIn: parent
                            text: "调谐"
                            color: "#071014"
                            font.pixelSize: 14
                            font.bold: true
                        }

                        MouseArea {
                            id: fmTuneMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: root.commitFmFrequency()
                        }
                    }

                    Text {
                        width: parent.width
                        visible: false
                        text: root.mhzText(fmReceiver.minFrequencyHz, 3)
                              + " - " + root.mhzText(fmReceiver.maxFrequencyHz, 3) + " MHz"
                        color: "#788895"
                        font.pixelSize: 12
                        elide: Text.ElideRight
                    }

                    Text {
                        width: parent.width
                        visible: false
                        text: "模式"
                        color: "#8d9daa"
                        font.pixelSize: 12
                    }

                    Row {
                        id: fmModeRow
                        width: parent.width
                        height: 32
                        spacing: 8

                        Repeater {
                            model: [
                                { "label": "WFM", "mode": "wfm" },
                                { "label": "NFM", "mode": "nfm" }
                            ]

                            Rectangle {
                                width: (fmModeRow.width - fmModeRow.spacing) / 2
                                height: fmModeRow.height
                                radius: 8
                                color: fmReceiver.mode === modelData.mode ? root.accent
                                      : fmModeMouse.pressed ? "#29323a"
                                      : fmModeMouse.containsMouse ? "#222a31" : "#101419"
                                border.color: fmReceiver.mode === modelData.mode ? root.accent : "#313c46"

                                Text {
                                    anchors.centerIn: parent
                                    text: modelData.label
                                    color: fmReceiver.mode === modelData.mode ? "#071014" : "#d8e2ea"
                                    font.pixelSize: 13
                                    font.bold: true
                                }

                                MouseArea {
                                    id: fmModeMouse
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: fmReceiver.setMode(modelData.mode)
                                }
                            }
                        }
                    }

                    Text {
                        width: parent.width
                        visible: false
                        text: "音量"
                        color: "#8d9daa"
                        font.pixelSize: 12
                    }

                    Rectangle {
                        id: fmVolumeTrack
                        width: parent.width
                        height: 32
                        radius: 8
                        color: "#0b0f13"
                        border.color: "#27313a"

                        Rectangle {
                            anchors.left: parent.left
                            anchors.top: parent.top
                            anchors.bottom: parent.bottom
                            anchors.margins: 3
                            width: Math.max(8, (parent.width - 6) * Math.min(1.0, fmReceiver.volume / 1.5))
                            radius: 6
                            color: root.accent
                            opacity: 0.86
                        }

                        Text {
                            anchors.centerIn: parent
                            text: Math.round(fmReceiver.volume * 100) + "%"
                            color: "#e6edf3"
                            font.pixelSize: 13
                            font.bold: true
                        }

                        MouseArea {
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor

                            function applyVolume(x) {
                                fmReceiver.setVolume(Math.max(0, Math.min(1.5, x / width * 1.5)))
                            }

                            onClicked: applyVolume(mouse.x)
                            onPositionChanged: {
                                if (pressed) {
                                    applyVolume(mouse.x)
                                }
                            }
                        }
                    }
                }

                Column {
                    width: parent.width
                    visible: root.appId === "record"
                    spacing: 6

                    Rectangle {
                        width: parent.width
                        height: 40
                        radius: 8
                        color: "#0b0f13"
                        border.color: recordFrequencyInput.activeFocus ? root.accent : "#27313a"

                        TextInput {
                            id: recordFrequencyInput
                            anchors.left: parent.left
                            anchors.leftMargin: 12
                            anchors.right: recordFrequencyUnit.left
                            anchors.rightMargin: 8
                            anchors.verticalCenter: parent.verticalCenter
                            text: root.mhzText(recordReceiver.centerFrequencyHz, 3)
                            color: "#eef6fb"
                            selectedTextColor: "#071014"
                            selectionColor: root.accent
                            font.pixelSize: 18
                            font.bold: true
                            selectByMouse: true
                            inputMethodHints: Qt.ImhFormattedNumbersOnly
                            onAccepted: root.commitRecordFrequency()
                        }

                        Text {
                            id: recordFrequencyUnit
                            anchors.right: parent.right
                            anchors.rightMargin: 12
                            anchors.verticalCenter: parent.verticalCenter
                            text: "MHz"
                            color: "#8d9daa"
                            font.pixelSize: 12
                        }
                    }

                    Row {
                        id: recordStepRow
                        width: parent.width
                        height: 30
                        spacing: 8

                        Repeater {
                            model: [
                                { "label": "-10", "step": -10 },
                                { "label": "-1", "step": -1 },
                                { "label": "+1", "step": 1 },
                                { "label": "+10", "step": 10 }
                            ]

                            Rectangle {
                                width: (recordStepRow.width - recordStepRow.spacing * 3) / 4
                                height: recordStepRow.height
                                radius: 8
                                color: recordStepMouse.pressed ? "#29323a" : recordStepMouse.containsMouse ? "#222a31" : "#101419"
                                border.color: "#313c46"

                                Text {
                                    anchors.centerIn: parent
                                    text: modelData.label
                                    color: "#d8e2ea"
                                    font.pixelSize: 13
                                    font.bold: true
                                }

                                MouseArea {
                                    id: recordStepMouse
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: {
                                        recordReceiver.stepMHz(modelData.step)
                                        recordFrequencyInput.text = root.mhzText(recordReceiver.centerFrequencyHz, 3)
                                    }
                                }
                            }
                        }
                    }

                    Rectangle {
                        width: parent.width
                        height: 32
                        radius: 8
                        color: recordTuneMouse.pressed ? Qt.darker(root.accent, 1.35) : recordTuneMouse.containsMouse ? Qt.lighter(root.accent, 1.08) : root.accent

                        Text {
                            anchors.centerIn: parent
                            text: "调谐"
                            color: "#071014"
                            font.pixelSize: 14
                            font.bold: true
                        }

                        MouseArea {
                            id: recordTuneMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: root.commitRecordFrequency()
                        }
                    }

                    Rectangle {
                        width: parent.width
                        height: 40
                        radius: 8
                        color: "#0b0f13"
                        border.color: recordSampleRateInput.activeFocus ? root.accent : "#27313a"

                        TextInput {
                            id: recordSampleRateInput
                            anchors.left: parent.left
                            anchors.leftMargin: 12
                            anchors.right: recordSampleRateUnit.left
                            anchors.rightMargin: 8
                            anchors.verticalCenter: parent.verticalCenter
                            text: (recordReceiver.sampleRateHz / 1000000).toFixed(3)
                            color: "#eef6fb"
                            selectedTextColor: "#071014"
                            selectionColor: root.accent
                            font.pixelSize: 16
                            font.bold: true
                            selectByMouse: true
                            inputMethodHints: Qt.ImhFormattedNumbersOnly
                            onAccepted: root.commitRecordSampleRate()
                        }

                        Text {
                            id: recordSampleRateUnit
                            anchors.right: parent.right
                            anchors.rightMargin: 12
                            anchors.verticalCenter: parent.verticalCenter
                            text: "MSPS"
                            color: "#8d9daa"
                            font.pixelSize: 12
                        }
                    }

                    Rectangle {
                        width: parent.width
                        height: 56
                        radius: 8
                        color: "#0b0f13"
                        border.color: recordPathInput.activeFocus ? root.accent : "#27313a"

                        TextInput {
                            id: recordPathInput
                            anchors.fill: parent
                            anchors.margins: 10
                            text: recordReceiver.outputFilePath
                            color: "#d8e2ea"
                            selectedTextColor: "#071014"
                            selectionColor: root.accent
                            font.pixelSize: 12
                            selectByMouse: true
                            clip: true
                            onAccepted: root.commitRecordOutputPath()
                        }
                    }

                    Row {
                        id: recordPathButtonRow
                        width: parent.width
                        height: 30
                        spacing: 8

                        Rectangle {
                            width: (parent.width - parent.spacing) / 2
                            height: parent.height
                            radius: 8
                            color: recordApplyPathMouse.pressed ? "#29323a" : recordApplyPathMouse.containsMouse ? "#222a31" : "#101419"
                            border.color: "#313c46"

                            Text {
                                anchors.centerIn: parent
                                text: "应用路径"
                                color: "#d8e2ea"
                                font.pixelSize: 13
                                font.bold: true
                            }

                            MouseArea {
                                id: recordApplyPathMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: root.commitRecordOutputPath()
                            }
                        }

                        Rectangle {
                            width: (parent.width - parent.spacing) / 2
                            height: parent.height
                            radius: 8
                            color: recordResetPathMouse.pressed ? "#29323a" : recordResetPathMouse.containsMouse ? "#222a31" : "#101419"
                            border.color: "#313c46"

                            Text {
                                anchors.centerIn: parent
                                text: "新文件名"
                                color: "#d8e2ea"
                                font.pixelSize: 13
                                font.bold: true
                            }

                            MouseArea {
                                id: recordResetPathMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    recordReceiver.resetOutputFilePath()
                                    recordPathInput.text = recordReceiver.outputFilePath
                                }
                            }
                        }
                    }
                }

                Column {
                    width: parent.width
                    visible: root.appId === "sensors"
                    spacing: 6

                    Row {
                        id: sensorBandRow
                        width: parent.width
                        height: 32
                        spacing: 8

                        Repeater {
                            model: [
                                { "label": "433.92", "band": "433" },
                                { "label": "315.00", "band": "315" },
                                { "label": "303.40", "band": "303" }
                            ]

                            Rectangle {
                                width: (sensorBandRow.width - sensorBandRow.spacing * 2) / 3
                                height: sensorBandRow.height
                                radius: 8
                                color: sensorReceiver.bandLabel.indexOf(modelData.band) === 0 ? root.accent
                                      : sensorBandMouse.pressed ? "#29323a"
                                      : sensorBandMouse.containsMouse ? "#222a31" : "#101419"
                                border.color: sensorReceiver.bandLabel.indexOf(modelData.band) === 0 ? root.accent : "#313c46"

                                Text {
                                    anchors.centerIn: parent
                                    text: modelData.label
                                    color: sensorReceiver.bandLabel.indexOf(modelData.band) === 0 ? "#071014" : "#d8e2ea"
                                    font.pixelSize: 13
                                    font.bold: true
                                }

                                MouseArea {
                                    id: sensorBandMouse
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: {
                                        sensorReceiver.setBand(modelData.band)
                                        sensorFrequencyInput.text = root.mhzText(sensorReceiver.centerFrequencyHz, 3)
                                    }
                                }
                            }
                        }
                    }

                    Rectangle {
                        width: parent.width
                        height: 40
                        radius: 8
                        color: "#0b0f13"
                        border.color: sensorFrequencyInput.activeFocus ? root.accent : "#27313a"

                        TextInput {
                            id: sensorFrequencyInput
                            anchors.left: parent.left
                            anchors.leftMargin: 12
                            anchors.right: sensorFrequencyUnit.left
                            anchors.rightMargin: 8
                            anchors.verticalCenter: parent.verticalCenter
                            text: root.mhzText(sensorReceiver.centerFrequencyHz, 3)
                            color: "#eef6fb"
                            selectedTextColor: "#071014"
                            selectionColor: root.accent
                            font.pixelSize: 18
                            font.bold: true
                            selectByMouse: true
                            inputMethodHints: Qt.ImhFormattedNumbersOnly
                            onAccepted: root.commitSensorFrequency()
                        }

                        Text {
                            id: sensorFrequencyUnit
                            anchors.right: parent.right
                            anchors.rightMargin: 12
                            anchors.verticalCenter: parent.verticalCenter
                            text: "MHz"
                            color: "#8d9daa"
                            font.pixelSize: 12
                        }
                    }

                    Row {
                        id: sensorStepRow
                        width: parent.width
                        height: 30
                        spacing: 8

                        Repeater {
                            model: [
                                { "label": "-1", "step": -1 },
                                { "label": "-0.1", "step": -0.1 },
                                { "label": "+0.1", "step": 0.1 },
                                { "label": "+1", "step": 1 }
                            ]

                            Rectangle {
                                width: (sensorStepRow.width - sensorStepRow.spacing * 3) / 4
                                height: sensorStepRow.height
                                radius: 8
                                color: sensorStepMouse.pressed ? "#29323a" : sensorStepMouse.containsMouse ? "#222a31" : "#101419"
                                border.color: "#313c46"

                                Text {
                                    anchors.centerIn: parent
                                    text: modelData.label
                                    color: "#d8e2ea"
                                    font.pixelSize: 13
                                    font.bold: true
                                }

                                MouseArea {
                                    id: sensorStepMouse
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: {
                                        sensorReceiver.stepMHz(modelData.step)
                                        sensorFrequencyInput.text = root.mhzText(sensorReceiver.centerFrequencyHz, 3)
                                    }
                                }
                            }
                        }
                    }

                    Row {
                        width: parent.width
                        height: 32
                        spacing: 8

                        Rectangle {
                            width: (parent.width - parent.spacing) / 2
                            height: parent.height
                            radius: 8
                            color: sensorTuneMouse.pressed ? Qt.darker(root.accent, 1.35) : sensorTuneMouse.containsMouse ? Qt.lighter(root.accent, 1.08) : root.accent

                            Text {
                                anchors.centerIn: parent
                                text: "调谐"
                                color: "#071014"
                                font.pixelSize: 14
                                font.bold: true
                            }

                            MouseArea {
                                id: sensorTuneMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: root.commitSensorFrequency()
                            }
                        }

                        Rectangle {
                            width: (parent.width - parent.spacing) / 2
                            height: parent.height
                            radius: 8
                            color: sensorClearMouse.pressed ? "#29323a" : sensorClearMouse.containsMouse ? "#222a31" : "#101419"
                            border.color: "#313c46"

                            Text {
                                anchors.centerIn: parent
                                text: "清空"
                                color: "#d8e2ea"
                                font.pixelSize: 14
                                font.bold: true
                            }

                            MouseArea {
                                id: sensorClearMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: sensorReceiver.clearEvents()
                            }
                        }
                    }
                }

                StatusPill {
                    width: parent.width
                    visible: root.appId === "waterfall"
                    label: "Tuner：" + waterfallReceiver.tunerName
                    accent: "#f4d35e"
                }

                StatusPill {
                    width: parent.width
                    visible: root.appId === "waterfall"
                    label: "采样：" + root.sampleRateText(waterfallReceiver.sampleRateHz)
                    accent: "#ff5c8a"
                }

                Column {
                    width: parent.width
                    visible: root.appId === "waterfall"
                    spacing: 8

                    Text {
                        width: parent.width
                        text: "中心频率 MHz"
                        color: "#8d9daa"
                        font.pixelSize: 12
                    }

                    Rectangle {
                        width: parent.width
                        height: 42
                        radius: 8
                        color: "#0b0f13"
                        border.color: waterfallFrequencyInput.activeFocus ? root.accent : "#27313a"

                        TextInput {
                            id: waterfallFrequencyInput
                            anchors.left: parent.left
                            anchors.leftMargin: 12
                            anchors.right: unitLabel.left
                            anchors.rightMargin: 8
                            anchors.verticalCenter: parent.verticalCenter
                            text: root.mhzText(waterfallReceiver.centerFrequencyHz, 3)
                            color: "#eef6fb"
                            selectedTextColor: "#071014"
                            selectionColor: root.accent
                            font.pixelSize: 18
                            font.bold: true
                            selectByMouse: true
                            inputMethodHints: Qt.ImhFormattedNumbersOnly
                            onAccepted: root.commitWaterfallFrequency()
                        }

                        Text {
                            id: unitLabel
                            anchors.right: parent.right
                            anchors.rightMargin: 12
                            anchors.verticalCenter: parent.verticalCenter
                            text: "MHz"
                            color: "#8d9daa"
                            font.pixelSize: 12
                        }
                    }

                    Row {
                        id: waterfallStepRow
                        width: parent.width
                        height: 32
                        spacing: 8

                        Repeater {
                            model: [
                                { "label": "-10", "step": -10 },
                                { "label": "-1", "step": -1 },
                                { "label": "+1", "step": 1 },
                                { "label": "+10", "step": 10 }
                            ]

                            Rectangle {
                                width: (waterfallStepRow.width - waterfallStepRow.spacing * 3) / 4
                                height: waterfallStepRow.height
                                radius: 8
                                color: stepMouse.pressed ? "#29323a" : stepMouse.containsMouse ? "#222a31" : "#101419"
                                border.color: "#313c46"

                                Text {
                                    anchors.centerIn: parent
                                    text: modelData.label
                                    color: "#d8e2ea"
                                    font.pixelSize: 13
                                    font.bold: true
                                }

                                MouseArea {
                                    id: stepMouse
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: {
                                        waterfallReceiver.stepMHz(modelData.step)
                                        waterfallFrequencyInput.text = root.mhzText(waterfallReceiver.centerFrequencyHz, 3)
                                    }
                                }
                            }
                        }
                    }

                    Rectangle {
                        width: parent.width
                        height: 36
                        radius: 8
                        color: tuneMouse.pressed ? Qt.darker(root.accent, 1.35) : tuneMouse.containsMouse ? Qt.lighter(root.accent, 1.08) : root.accent

                        Text {
                            anchors.centerIn: parent
                            text: "调谐"
                            color: "#071014"
                            font.pixelSize: 14
                            font.bold: true
                        }

                        MouseArea {
                            id: tuneMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: root.commitWaterfallFrequency()
                        }
                    }

                    Text {
                        width: parent.width
                        text: root.mhzText(waterfallReceiver.minFrequencyHz, 3)
                              + " - " + root.mhzText(waterfallReceiver.maxFrequencyHz, 3) + " MHz"
                        color: "#788895"
                        font.pixelSize: 12
                        elide: Text.ElideRight
                    }
                }

                StatusPill {
                    width: parent.width
                    visible: root.appId === "adsb"
                    label: "飞机：" + adsbReceiver.aircraftCount + " 架"
                    accent: "#f4d35e"
                }

                StatusPill {
                    width: parent.width
                    visible: root.appId === "adsb"
                    label: "有效消息：" + adsbReceiver.validMessages
                    accent: "#ff5c8a"
                }

                // ---- Scanner controls ----
                StatusPill {
                    width: parent.width
                    visible: root.appId === "scanner"
                    label: "Tuner：" + scannerReceiver.tunerName
                    accent: "#f4d35e"
                }

                StatusPill {
                    width: parent.width
                    visible: root.appId === "scanner"
                    label: "采样：" + root.sampleRateText(scannerReceiver.sampleRateHz)
                    accent: "#ff5c8a"
                }

                StatusPill {
                    width: parent.width
                    visible: root.appId === "scanner"
                    label: "频点：" + scannerReceiver.totalBins + " 个"
                    accent: "#a855f7"
                }

                Column {
                    width: parent.width
                    visible: root.appId === "scanner"
                    spacing: 8

                    Text {
                        width: parent.width
                        text: "起始频率 MHz"
                        color: "#8d9daa"
                        font.pixelSize: 12
                    }

                    Rectangle {
                        width: parent.width
                        height: 40
                        radius: 8
                        color: "#0b0f13"
                        border.color: scannerStartInput.activeFocus ? root.accent : "#27313a"

                        TextInput {
                            id: scannerStartInput
                            anchors.left: parent.left
                            anchors.leftMargin: 12
                            anchors.right: scannerStartUnit.left
                            anchors.rightMargin: 8
                            anchors.verticalCenter: parent.verticalCenter
                            text: root.mhzText(scannerReceiver.startFrequencyHz, 1)
                            color: "#eef6fb"
                            selectedTextColor: "#071014"
                            selectionColor: root.accent
                            font.pixelSize: 18
                            font.bold: true
                            selectByMouse: true
                            inputMethodHints: Qt.ImhFormattedNumbersOnly
                            onAccepted: root.commitScannerRange()
                        }

                        Text {
                            id: scannerStartUnit
                            anchors.right: parent.right
                            anchors.rightMargin: 12
                            anchors.verticalCenter: parent.verticalCenter
                            text: "MHz"
                            color: "#8d9daa"
                            font.pixelSize: 12
                        }
                    }

                    Text {
                        width: parent.width
                        text: "终止频率 MHz"
                        color: "#8d9daa"
                        font.pixelSize: 12
                    }

                    Rectangle {
                        width: parent.width
                        height: 40
                        radius: 8
                        color: "#0b0f13"
                        border.color: scannerStopInput.activeFocus ? root.accent : "#27313a"

                        TextInput {
                            id: scannerStopInput
                            anchors.left: parent.left
                            anchors.leftMargin: 12
                            anchors.right: scannerStopUnit.left
                            anchors.rightMargin: 8
                            anchors.verticalCenter: parent.verticalCenter
                            text: root.mhzText(scannerReceiver.stopFrequencyHz, 1)
                            color: "#eef6fb"
                            selectedTextColor: "#071014"
                            selectionColor: root.accent
                            font.pixelSize: 18
                            font.bold: true
                            selectByMouse: true
                            inputMethodHints: Qt.ImhFormattedNumbersOnly
                            onAccepted: root.commitScannerRange()
                        }

                        Text {
                            id: scannerStopUnit
                            anchors.right: parent.right
                            anchors.rightMargin: 12
                            anchors.verticalCenter: parent.verticalCenter
                            text: "MHz"
                            color: "#8d9daa"
                            font.pixelSize: 12
                        }
                    }

                    Row {
                        id: scannerBandRow
                        width: parent.width
                        height: 30
                        spacing: 6

                        Repeater {
                            model: [
                                { "label": "FM",  "start": 87.5,  "stop": 108.0 },
                                { "label": "航空", "start": 108.0, "stop": 137.0 },
                                { "label": "433", "start": 430.0, "stop": 440.0 }
                            ]

                            Rectangle {
                                width: (scannerBandRow.width - scannerBandRow.spacing * 2) / 3
                                height: scannerBandRow.height
                                radius: 8
                                color: scannerBandMouse.pressed ? "#29323a"
                                     : scannerBandMouse.containsMouse ? "#222a31" : "#101419"
                                border.color: "#313c46"

                                Text {
                                    anchors.centerIn: parent
                                    text: modelData.label
                                    color: "#d8e2ea"
                                    font.pixelSize: 13
                                    font.bold: true
                                }

                                MouseArea {
                                    id: scannerBandMouse
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: {
                                        scannerReceiver.setStartFrequencyHz(modelData.start * 1000000)
                                        scannerReceiver.setStopFrequencyHz(modelData.stop  * 1000000)
                                        scannerStartInput.text = root.mhzText(scannerReceiver.startFrequencyHz, 1)
                                        scannerStopInput.text  = root.mhzText(scannerReceiver.stopFrequencyHz,  1)
                                    }
                                }
                            }
                        }
                    }

                    Rectangle {
                        width: parent.width
                        height: 36
                        radius: 8
                        color: scannerApplyMouse.pressed ? Qt.darker(root.accent, 1.35)
                             : scannerApplyMouse.containsMouse ? Qt.lighter(root.accent, 1.08)
                             : root.accent

                        Text {
                            anchors.centerIn: parent
                            text: "应用范围"
                            color: "#071014"
                            font.pixelSize: 14
                            font.bold: true
                        }

                        MouseArea {
                            id: scannerApplyMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: root.commitScannerRange()
                        }
                    }

                    Text {
                        width: parent.width
                        text: root.mhzText(scannerReceiver.minFrequencyHz, 1)
                              + " - " + root.mhzText(scannerReceiver.maxFrequencyHz, 1) + " MHz"
                        color: "#788895"
                        font.pixelSize: 12
                        elide: Text.ElideRight
                    }
                }
            }

            Item {
                id: trackToggleRow
                anchors.left: panelTitle.left
                anchors.right: panelTitle.right
                anchors.top: metricsColumn.bottom
                anchors.topMargin: 18
                height: 24
                visible: root.appId === "adsb"

                Text {
                    anchors.left: parent.left
                    anchors.right: trackSwitch.left
                    anchors.rightMargin: 10
                    anchors.verticalCenter: parent.verticalCenter
                    text: "航迹"
                    color: "#8d9daa"
                    font.pixelSize: 12
                    elide: Text.ElideRight
                }

                Rectangle {
                    id: trackSwitch
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    width: 46
                    height: 24
                    radius: 12
                    color: root.adsbShowTracks ? root.accent : "#27313a"
                    border.color: root.adsbShowTracks ? root.accent : "#33444f"

                    Rectangle {
                        width: 20
                        height: 20
                        radius: 10
                        x: root.adsbShowTracks ? parent.width - width - 2 : 2
                        y: 2
                        color: "#eef6fb"

                        Behavior on x {
                            NumberAnimation {
                                duration: 120
                            }
                        }
                    }

                    MouseArea {
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.adsbShowTracks = !root.adsbShowTracks
                    }
                }
            }

            Text {
                id: aircraftListTitle
                anchors.left: panelTitle.left
                anchors.right: panelTitle.right
                anchors.top: trackToggleRow.bottom
                anchors.topMargin: 16
                visible: root.appId === "adsb"
                text: "航班信息"
                color: "#f3f6f8"
                font.pixelSize: 16
                font.bold: true
            }

            ListView {
                id: aircraftList
                anchors.left: panelTitle.left
                anchors.right: panelTitle.right
                anchors.top: aircraftListTitle.bottom
                anchors.topMargin: 10
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 18
                visible: root.appId === "adsb"
                model: root.appId === "adsb" ? adsbReceiver.aircraftModel : null
                spacing: 8
                clip: true

                delegate: Rectangle {
                    id: aircraftRow

                    property var aircraftInfo: ({
                        "icao": icao,
                        "callsign": callsign,
                        "altitude": altitude,
                        "hasAltitude": hasAltitude,
                        "latitude": latitude,
                        "longitude": longitude,
                        "hasPosition": hasPosition,
                        "speed": speed,
                        "hasSpeed": hasSpeed,
                        "heading": heading,
                        "hasHeading": hasHeading,
                        "messages": messages,
                        "lastSeenSeconds": lastSeenSeconds,
                        "track": track
                    })

                    width: aircraftList.width
                    height: 68
                    radius: 8
                    color: aircraftRowMouse.containsMouse || aircraftList.currentIndex === index ? "#101820" : "#0b0f13"
                    border.color: aircraftList.currentIndex === index ? root.accent : "#27313a"

                    Text {
                        id: flightTitle
                        anchors.left: parent.left
                        anchors.leftMargin: 12
                        anchors.right: ageLabel.left
                        anchors.rightMargin: 10
                        anchors.top: parent.top
                        anchors.topMargin: 10
                        text: callsign.length > 0 ? callsign : icao
                        color: "#eef6fb"
                        font.pixelSize: 14
                        font.bold: true
                        elide: Text.ElideRight
                    }

                    Text {
                        id: ageLabel
                        anchors.right: parent.right
                        anchors.rightMargin: 12
                        anchors.verticalCenter: flightTitle.verticalCenter
                        text: lastSeenSeconds + " 秒前"
                        color: "#7f93a2"
                        font.pixelSize: 11
                    }

                    Text {
                        anchors.left: flightTitle.left
                        anchors.right: parent.right
                        anchors.rightMargin: 12
                        anchors.top: flightTitle.bottom
                        anchors.topMargin: 7
                        text: (hasAltitude ? root.metersText(altitude) : "高度未知")
                              + " · " + (hasSpeed ? root.kmhText(speed) : "速度未知")
                              + " · " + (hasHeading ? Math.round(heading) + "°" : "航向未知")
                        color: "#a8bac6"
                        font.pixelSize: 12
                        elide: Text.ElideRight
                    }

                    Text {
                        anchors.left: flightTitle.left
                        anchors.right: parent.right
                        anchors.rightMargin: 12
                        anchors.bottom: parent.bottom
                        anchors.bottomMargin: 8
                        text: hasPosition ? latitude.toFixed(4) + ", " + longitude.toFixed(4) : icao
                        color: "#748796"
                        font.pixelSize: 11
                        elide: Text.ElideRight
                    }

                    MouseArea {
                        id: aircraftRowMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            aircraftList.currentIndex = index
                            root.focusAircraft(aircraftRow.aircraftInfo)
                        }
                    }
                }
            }
        }
    }

    Component {
        id: radarComponent
        RadarMapView {
            accent: root.accent
            aircraftModel: adsbReceiver.aircraftModel
            aircraftCount: adsbReceiver.aircraftCount
            centerLatitude: adsbReceiver.mapCenterLatitude
            centerLongitude: adsbReceiver.mapCenterLongitude
            showTracks: root.adsbShowTracks
        }
    }

    Component {
        id: amComponent
        Rectangle {
            color: "#0f1418"
            radius: 8
            border.color: "#2b3640"

            Column {
                anchors.centerIn: parent
                width: Math.min(parent.width - 72, 680)
                spacing: 22

                Text {
                    width: parent.width
                    horizontalAlignment: Text.AlignHCenter
                    text: root.mhzText(amReceiver.centerFrequencyHz, 3) + " MHz"
                    color: "#f6f8fb"
                    font.pixelSize: 52
                    font.bold: true
                    elide: Text.ElideRight
                }

                Text {
                    width: parent.width
                    horizontalAlignment: Text.AlignHCenter
                    text: "AM 包络检波 / " + (amReceiver.running ? "播放中" : "停止")
                    color: amReceiver.running ? root.accent : "#8fa0ad"
                    font.pixelSize: 17
                    font.bold: true
                    elide: Text.ElideRight
                }

                Rectangle {
                    width: parent.width
                    height: 82
                    radius: 8
                    color: "#11181e"
                    border.color: "#29343d"

                    Rectangle {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.leftMargin: 28
                        anchors.rightMargin: 28
                        height: 2
                        color: "#32414b"
                    }

                    Repeater {
                        model: 11

                        Rectangle {
                            width: 2
                            height: index % 2 === 0 ? 34 : 20
                            x: 28 + index * ((parent.width - 56) / 10) - width / 2
                            anchors.verticalCenter: parent.verticalCenter
                            color: index === 5 ? root.accent : "#657784"
                        }
                    }

                    Rectangle {
                        width: 4
                        height: 58
                        radius: 2
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.verticalCenter: parent.verticalCenter
                        color: root.accent
                    }
                }

                Row {
                    id: amMeterRow
                    width: parent.width
                    height: 30
                    spacing: 6

                    Repeater {
                        model: 18

                        Rectangle {
                            width: (amMeterRow.width - amMeterRow.spacing * 17) / 18
                            height: amMeterRow.height
                            radius: 4
                            color: index < Math.round(amReceiver.audioLevel * 18) ? root.accent : "#22303a"
                            opacity: index < Math.round(amReceiver.audioLevel * 18) ? 1.0 : 0.72
                        }
                    }
                }

                Text {
                    width: parent.width
                    horizontalAlignment: Text.AlignHCenter
                    text: amReceiver.status
                    color: "#a9bac6"
                    font.pixelSize: 14
                    elide: Text.ElideRight
                }
            }
        }
    }

    Component {
        id: fmComponent
        Rectangle {
            color: "#0f1418"
            radius: 8
            border.color: "#2b3640"

            Column {
                anchors.centerIn: parent
                width: Math.min(parent.width - 72, 680)
                spacing: 22

                Text {
                    width: parent.width
                    horizontalAlignment: Text.AlignHCenter
                    text: root.mhzText(fmReceiver.centerFrequencyHz, 3) + " MHz"
                    color: "#f6f8fb"
                    font.pixelSize: 52
                    font.bold: true
                    elide: Text.ElideRight
                }

                Text {
                    width: parent.width
                    horizontalAlignment: Text.AlignHCenter
                    text: fmReceiver.modeLabel + " / " + (fmReceiver.running ? "播放中" : "停止")
                    color: fmReceiver.running ? root.accent : "#8fa0ad"
                    font.pixelSize: 17
                    font.bold: true
                    elide: Text.ElideRight
                }

                Rectangle {
                    width: parent.width
                    height: 82
                    radius: 8
                    color: "#11181e"
                    border.color: "#29343d"

                    Rectangle {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.leftMargin: 28
                        anchors.rightMargin: 28
                        height: 2
                        color: "#32414b"
                    }

                    Repeater {
                        model: 13

                        Rectangle {
                            width: 2
                            height: index % 3 === 0 ? 34 : 20
                            x: 28 + index * ((parent.width - 56) / 12) - width / 2
                            anchors.verticalCenter: parent.verticalCenter
                            color: index === 6 ? root.accent : "#657784"
                        }
                    }

                    Rectangle {
                        width: 4
                        height: 58
                        radius: 2
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.verticalCenter: parent.verticalCenter
                        color: root.accent
                    }
                }

                Row {
                    id: fmMeterRow
                    width: parent.width
                    height: 30
                    spacing: 6

                    Repeater {
                        model: 18

                        Rectangle {
                            width: (fmMeterRow.width - fmMeterRow.spacing * 17) / 18
                            height: fmMeterRow.height
                            radius: 4
                            color: index < Math.round(fmReceiver.audioLevel * 18) ? root.accent : "#22303a"
                            opacity: index < Math.round(fmReceiver.audioLevel * 18) ? 1.0 : 0.72
                        }
                    }
                }

                Text {
                    width: parent.width
                    horizontalAlignment: Text.AlignHCenter
                    text: fmReceiver.status
                    color: "#a9bac6"
                    font.pixelSize: 14
                    elide: Text.ElideRight
                }
            }
        }
    }

    Component {
        id: waterfallComponent
        WaterfallView {
            accent: root.accent
            receiver: waterfallReceiver
        }
    }

    Component {
        id: sensorComponent
        SensorDecodeView {
            accent: root.accent
            receiver: sensorReceiver
        }
    }

    Component {
        id: recordComponent
        Rectangle {
            color: "#0f1418"
            radius: 8
            border.color: "#2b3640"

            Column {
                anchors.centerIn: parent
                width: Math.min(parent.width - 72, 760)
                spacing: 20

                Text {
                    width: parent.width
                    horizontalAlignment: Text.AlignHCenter
                    text: root.mhzText(recordReceiver.centerFrequencyHz, 3) + " MHz"
                    color: "#f6f8fb"
                    font.pixelSize: 52
                    font.bold: true
                    elide: Text.ElideRight
                }

                Text {
                    width: parent.width
                    horizontalAlignment: Text.AlignHCenter
                    text: "u8 I/Q / " + root.sampleRateText(recordReceiver.sampleRateHz) + " / " + (recordReceiver.running ? "录制中" : "停止")
                    color: recordReceiver.running ? root.accent : "#8fa0ad"
                    font.pixelSize: 17
                    font.bold: true
                    elide: Text.ElideRight
                }

                Row {
                    width: parent.width
                    height: 64
                    spacing: 18

                    Column {
                        width: (parent.width - parent.spacing * 2) / 3
                        height: parent.height
                        spacing: 8

                        Text {
                            width: parent.width
                            horizontalAlignment: Text.AlignHCenter
                            text: "文件大小"
                            color: "#8293a0"
                            font.pixelSize: 12
                        }

                        Text {
                            width: parent.width
                            horizontalAlignment: Text.AlignHCenter
                            text: root.bytesText(recordReceiver.bytesWritten)
                            color: "#eef6fb"
                            font.pixelSize: 24
                            font.bold: true
                            elide: Text.ElideRight
                        }
                    }

                    Column {
                        width: (parent.width - parent.spacing * 2) / 3
                        height: parent.height
                        spacing: 8

                        Text {
                            width: parent.width
                            horizontalAlignment: Text.AlignHCenter
                            text: "录制时长"
                            color: "#8293a0"
                            font.pixelSize: 12
                        }

                        Text {
                            width: parent.width
                            horizontalAlignment: Text.AlignHCenter
                            text: root.durationText(recordReceiver.elapsedSeconds)
                            color: "#eef6fb"
                            font.pixelSize: 24
                            font.bold: true
                            elide: Text.ElideRight
                        }
                    }

                    Column {
                        width: (parent.width - parent.spacing * 2) / 3
                        height: parent.height
                        spacing: 8

                        Text {
                            width: parent.width
                            horizontalAlignment: Text.AlignHCenter
                            text: "Tuner"
                            color: "#8293a0"
                            font.pixelSize: 12
                        }

                        Text {
                            width: parent.width
                            horizontalAlignment: Text.AlignHCenter
                            text: recordReceiver.tunerName
                            color: "#eef6fb"
                            font.pixelSize: 16
                            font.bold: true
                            elide: Text.ElideRight
                        }
                    }
                }

                Rectangle {
                    width: parent.width
                    height: 96
                    radius: 8
                    color: "#11181e"
                    border.color: "#29343d"

                    Text {
                        id: recordPathLabel
                        anchors.left: parent.left
                        anchors.leftMargin: 16
                        anchors.right: parent.right
                        anchors.rightMargin: 16
                        anchors.top: parent.top
                        anchors.topMargin: 12
                        text: "保存位置"
                        color: "#8293a0"
                        font.pixelSize: 12
                        elide: Text.ElideRight
                    }

                    Text {
                        anchors.left: recordPathLabel.left
                        anchors.right: recordPathLabel.right
                        anchors.top: recordPathLabel.bottom
                        anchors.topMargin: 9
                        anchors.bottom: parent.bottom
                        anchors.bottomMargin: 12
                        text: recordReceiver.savedLocation
                        color: "#d8e2ea"
                        font.pixelSize: 14
                        wrapMode: Text.Wrap
                        maximumLineCount: 2
                        elide: Text.ElideMiddle
                    }
                }

                Text {
                    width: parent.width
                    horizontalAlignment: Text.AlignHCenter
                    text: recordReceiver.status
                    color: "#a9bac6"
                    font.pixelSize: 14
                    elide: Text.ElideRight
                }
            }
        }
    }

    Component {
        id: scannerComponent
        HeatmapView {
            accent: root.accent
            receiver: scannerReceiver
        }
    }

    Component {
        id: receiverComponent
        Rectangle {
            color: "#0f1418"
            radius: 8
            border.color: "#2b3640"

            IconGlyph {
                id: centerIcon
                anchors.centerIn: parent
                anchors.verticalCenterOffset: -42
                width: 96
                height: 96
                kind: root.iconKind
                accent: root.accent
            }

            Text {
                anchors.top: centerIcon.bottom
                anchors.topMargin: 26
                anchors.horizontalCenter: parent.horizontalCenter
                width: parent.width - 64
                horizontalAlignment: Text.AlignHCenter
                text: root.appId === "record" ? "射频 I/Q 录制模块预留界面" : "接收机控制模块预留界面"
                color: "#e6edf3"
                font.pixelSize: 22
                font.bold: true
                wrapMode: Text.Wrap
            }

            Text {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 34
                horizontalAlignment: Text.AlignHCenter
                text: "后续可接入 rtl-sdr、DSP 解调链路和音频输出"
                color: "#8fa0ad"
                font.pixelSize: 14
            }
        }
    }
}
