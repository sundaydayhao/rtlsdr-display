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
    property bool recordPlaybackMode: false
    property bool compactUi: width <= 820 || height <= 460
    property int edgeMargin: compactUi ? 14 : 22
    property int contentTopMargin: compactUi ? 62 : 92
    property int contentBottomMargin: compactUi ? 12 : 34
    property int sidePanelGap: compactUi ? 14 : 22

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
        anchors.topMargin: root.compactUi ? 6 : 12
        anchors.bottomMargin: root.compactUi ? 8 : 18
        radius: 8
        color: "#151a1f"
        border.color: "#29343d"
    }

    Rectangle {
        id: backButton
        anchors.left: parent.left
        anchors.leftMargin: root.compactUi ? 14 : 18
        anchors.top: parent.top
        anchors.topMargin: root.compactUi ? 16 : 30
        width: root.compactUi ? 38 : 44
        height: root.compactUi ? 32 : 36
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
        anchors.leftMargin: root.compactUi ? 10 : 16
        anchors.verticalCenter: backButton.verticalCenter
        width: root.compactUi ? 36 : 44
        height: root.compactUi ? 36 : 44
        radius: 8
        color: Qt.rgba(root.accent.r, root.accent.g, root.accent.b, 0.16)
        border.color: Qt.rgba(root.accent.r, root.accent.g, root.accent.b, 0.44)

        IconGlyph {
            anchors.centerIn: parent
            width: root.compactUi ? 24 : 30
            height: root.compactUi ? 24 : 30
            kind: root.iconKind
            accent: root.accent
        }
    }

    Text {
        id: titleLabel
        anchors.left: iconPlate.right
        anchors.leftMargin: root.compactUi ? 10 : 14
        anchors.right: launchButton.left
        anchors.rightMargin: root.compactUi ? 10 : 18
        anchors.top: parent.top
        anchors.topMargin: root.compactUi ? 14 : 28
        text: root.title
        color: "#f6f8fb"
        font.pixelSize: root.compactUi ? 20 : 24
        font.bold: true
        elide: Text.ElideRight
    }

    Text {
        anchors.left: titleLabel.left
        anchors.right: titleLabel.right
        anchors.top: titleLabel.bottom
        anchors.topMargin: root.compactUi ? 3 : 5
        text: root.subtitle
        color: "#9baab7"
        font.pixelSize: root.compactUi ? 12 : 13
        elide: Text.ElideRight
    }

    Rectangle {
        id: launchButton
        anchors.right: parent.right
        anchors.rightMargin: root.compactUi ? 14 : 22
        anchors.verticalCenter: backButton.verticalCenter
        width: root.compactUi ? 92 : 112
        height: root.compactUi ? 34 : 40
        radius: 8
        color: launchMouse.pressed ? Qt.darker(root.accent, 1.35) : launchMouse.containsMouse ? Qt.lighter(root.accent, 1.08) : root.accent

        Text {
            anchors.centerIn: parent
            text: (root.appId === "adsb" && adsbReceiver.running)
                  || (root.appId === "fm" && fmReceiver.running)
                  || (root.appId === "record" && !root.recordPlaybackMode && recordReceiver.running)
                  || (root.appId === "record" && root.recordPlaybackMode && recordReceiver.playbackRunning)
                  || (root.appId === "sensors" && sensorReceiver.running)
                  || (root.appId === "waterfall" && waterfallReceiver.running)
                  || (root.appId === "scanner" && scannerReceiver.running) ? "停止" : "启动"
            color: "#071014"
            font.pixelSize: root.compactUi ? 14 : 15
            font.bold: true
        }

        MouseArea {
            id: launchMouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: {
                if (root.appId === "record" && root.recordPlaybackMode) {
                    if (recordReceiver.playbackRunning) {
                        recordReceiver.stopPlayback()
                    } else {
                        recordReceiver.startPlayback()
                    }
                } else {
                    root.startRequested(root.appId)
                }
            }
        }
    }

    Item {
        id: content
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.leftMargin: root.edgeMargin
        anchors.rightMargin: root.edgeMargin
        anchors.topMargin: root.contentTopMargin
        anchors.bottomMargin: root.contentBottomMargin

        Loader {
            id: visualLoader
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            width: Math.max(root.compactUi ? 320 : 420, parent.width - sidePanel.width - root.sidePanelGap)
            sourceComponent: root.appId === "adsb" ? radarComponent
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
            width: Math.min(root.compactUi ? 252 : 360, parent.width * (root.compactUi ? 0.34 : 0.32))
            radius: 8
            color: "#101419"
            border.color: "#2b3640"
            clip: true

            Text {
                id: panelTitle
                anchors.left: parent.left
                anchors.leftMargin: root.compactUi ? 12 : 18
                anchors.right: parent.right
                anchors.rightMargin: root.compactUi ? 12 : 18
                anchors.top: parent.top
                anchors.topMargin: root.compactUi ? 12 : 18
                text: root.appId === "adsb" ? "ADS-B 接收"
                      : root.appId === "fm" ? "FM 接收"
                      : root.appId === "record" ? "I/Q 录制"
                      : root.appId === "sensors" ? "无线报文解码"
                      : root.appId === "waterfall" ? "瀑布图接收"
                      : root.appId === "scanner" ? "宽带扫描"
                      : "运行配置"
                color: "#f3f6f8"
                font.pixelSize: root.compactUi ? 16 : 18
                font.bold: true
            }

            Column {
                id: metricsColumn
                anchors.left: panelTitle.left
                anchors.right: panelTitle.right
                anchors.top: panelTitle.bottom
                anchors.topMargin: root.compactUi ? 10 : 14
                spacing: root.compactUi ? 6 : (root.appId === "fm" || root.appId === "record" ? 8 : 12)

                Column {
                    width: parent.width
                    visible: root.appId === "fm"
                    spacing: 6

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

                    Rectangle {
                        id: fmAudioOutputSelector
                        property bool expanded: false

                        width: parent.width
                        height: expanded ? 42 + Math.min(5, fmReceiver.audioOutputDevices.length) * 34 : 38
                        radius: 8
                        color: "#0b0f13"
                        border.color: expanded ? root.accent : "#27313a"
                        clip: true

                        Rectangle {
                            id: fmAudioOutputHeader
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.top: parent.top
                            height: 38
                            color: fmAudioOutputMouse.pressed ? "#202830"
                                  : fmAudioOutputMouse.containsMouse ? "#161d23" : "transparent"

                            Column {
                                anchors.left: parent.left
                                anchors.leftMargin: 12
                                anchors.right: fmAudioOutputArrow.left
                                anchors.rightMargin: 8
                                anchors.verticalCenter: parent.verticalCenter
                                spacing: 1

                                Text {
                                    width: parent.width
                                    text: "音频输出"
                                    color: "#7f909d"
                                    font.pixelSize: 10
                                    elide: Text.ElideRight
                                }

                                Text {
                                    width: parent.width
                                    text: fmReceiver.audioOutputDeviceLabel
                                    color: "#e6edf3"
                                    font.pixelSize: 12
                                    font.bold: true
                                    elide: Text.ElideRight
                                }
                            }

                            Text {
                                id: fmAudioOutputArrow
                                anchors.right: parent.right
                                anchors.rightMargin: 12
                                anchors.verticalCenter: parent.verticalCenter
                                text: fmAudioOutputSelector.expanded ? "▲" : "▼"
                                color: "#8d9daa"
                                font.pixelSize: 10
                            }

                            MouseArea {
                                id: fmAudioOutputMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    fmReceiver.refreshAudioOutputDevices()
                                    fmAudioOutputSelector.expanded = !fmAudioOutputSelector.expanded
                                }
                            }
                        }

                        ListView {
                            id: fmAudioOutputList
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.top: fmAudioOutputHeader.bottom
                            height: Math.min(5, fmReceiver.audioOutputDevices.length) * 34
                            clip: true
                            interactive: fmReceiver.audioOutputDevices.length > 5
                            model: fmReceiver.audioOutputDevices
                            visible: fmAudioOutputSelector.expanded

                            delegate: Rectangle {
                                width: fmAudioOutputList.width
                                height: 34
                                color: fmAudioOutputOptionMouse.pressed ? "#29323a"
                                      : modelData.id === fmReceiver.audioOutputDeviceId ? "#18323a"
                                      : fmAudioOutputOptionMouse.containsMouse ? "#182129" : "#0b0f13"

                                Text {
                                    anchors.left: parent.left
                                    anchors.leftMargin: 12
                                    anchors.right: parent.right
                                    anchors.rightMargin: 12
                                    anchors.verticalCenter: parent.verticalCenter
                                    text: modelData.label
                                    color: modelData.id === fmReceiver.audioOutputDeviceId ? root.accent : "#d8e2ea"
                                    font.pixelSize: 12
                                    font.bold: modelData.id === fmReceiver.audioOutputDeviceId
                                    elide: Text.ElideRight
                                }

                                MouseArea {
                                    id: fmAudioOutputOptionMouse
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: {
                                        fmReceiver.setAudioOutputDeviceId(modelData.id)
                                        fmAudioOutputSelector.expanded = false
                                    }
                                }
                            }
                        }
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

                    // 录制 / 回放 模式切换
                    Row {
                        id: recordModeRow
                        width: parent.width
                        height: 32
                        spacing: 8

                        Rectangle {
                            width: (parent.width - parent.spacing) / 2
                            height: parent.height
                            radius: 8
                            color: !root.recordPlaybackMode ? root.accent
                                 : recModeMouse1.pressed ? "#29323a"
                                 : recModeMouse1.containsMouse ? "#222a31" : "#101419"
                            border.color: !root.recordPlaybackMode ? root.accent : "#313c46"

                            Text {
                                anchors.centerIn: parent
                                text: "录制"
                                color: !root.recordPlaybackMode ? "#071014" : "#d8e2ea"
                                font.pixelSize: 13
                                font.bold: true
                            }

                            MouseArea {
                                id: recModeMouse1
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: root.recordPlaybackMode = false
                            }
                        }

                        Rectangle {
                            width: (parent.width - parent.spacing) / 2
                            height: parent.height
                            radius: 8
                            color: root.recordPlaybackMode ? root.accent
                                 : recModeMouse2.pressed ? "#29323a"
                                 : recModeMouse2.containsMouse ? "#222a31" : "#101419"
                            border.color: root.recordPlaybackMode ? root.accent : "#313c46"

                            Text {
                                anchors.centerIn: parent
                                text: "回放"
                                color: root.recordPlaybackMode ? "#071014" : "#d8e2ea"
                                font.pixelSize: 13
                                font.bold: true
                            }

                            MouseArea {
                                id: recModeMouse2
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: root.recordPlaybackMode = true
                            }
                        }
                    }

                    // ---- 录制控件（录制模式）----
                    Rectangle {
                        width: parent.width
                        height: 40
                        visible: !root.recordPlaybackMode
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
                        visible: !root.recordPlaybackMode
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

                    // ---- 回放控件（回放模式）----
                    Text {
                        width: parent.width
                        visible: root.recordPlaybackMode
                        text: "回放文件路径"
                        color: "#8d9daa"
                        font.pixelSize: 12
                    }

                    Rectangle {
                        width: parent.width
                        height: 56
                        visible: root.recordPlaybackMode
                        radius: 8
                        color: "#0b0f13"
                        border.color: playbackFileInput.activeFocus ? root.accent : "#27313a"

                        TextInput {
                            id: playbackFileInput
                            anchors.fill: parent
                            anchors.margins: 10
                            text: recordReceiver.playbackFilePath
                            color: "#d8e2ea"
                            selectedTextColor: "#071014"
                            selectionColor: root.accent
                            font.pixelSize: 12
                            selectByMouse: true
                            clip: true
                            onAccepted: recordReceiver.setPlaybackFilePath(text)
                        }
                    }

                    Rectangle {
                        width: parent.width
                        height: 30
                        visible: root.recordPlaybackMode
                        radius: 8
                        color: applyPlaybackMouse.pressed ? "#29323a"
                             : applyPlaybackMouse.containsMouse ? "#222a31" : "#101419"
                        border.color: "#313c46"

                        Text {
                            anchors.centerIn: parent
                            text: "应用文件路径"
                            color: "#d8e2ea"
                            font.pixelSize: 13
                            font.bold: true
                        }

                        MouseArea {
                            id: applyPlaybackMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: recordReceiver.setPlaybackFilePath(playbackFileInput.text)
                        }
                    }

                    // 回放进度条（可点击跳转）
                    Rectangle {
                        id: playbackProgressBar
                        width: parent.width
                        height: 32
                        visible: root.recordPlaybackMode
                        radius: 8
                        color: "#0b0f13"
                        border.color: progressBarMouse.containsMouse ? root.accent : "#27313a"

                        Rectangle {
                            anchors.left: parent.left
                            anchors.top: parent.top
                            anchors.bottom: parent.bottom
                            anchors.margins: 3
                            width: Math.max(8, (parent.width - 6) * recordReceiver.playbackProgress)
                            radius: 6
                            color: root.accent
                            opacity: 0.86
                        }

                        Text {
                            anchors.centerIn: parent
                            text: recordReceiver.playbackRunning
                                  ? (Math.round(recordReceiver.playbackProgress * 100) + "% · "
                                     + root.durationText(recordReceiver.playbackElapsedSeconds)
                                     + " / " + root.durationText(recordReceiver.playbackDurationSeconds))
                                  : "回放进度（点击跳转）"
                            color: "#e6edf3"
                            font.pixelSize: 12
                            font.bold: true
                            elide: Text.ElideRight
                        }

                        MouseArea {
                            id: progressBarMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor

                            function seekTo(x) {
                                var progress = Math.max(0.0, Math.min(1.0, x / width))
                                recordReceiver.seekPlayback(progress)
                            }

                            onClicked: seekTo(mouse.x)
                            onPositionChanged: {
                                if (pressed) seekTo(mouse.x)
                            }
                        }
                    }

                    // 回放速度控制
                    Row {
                        id: playbackSpeedRow
                        width: parent.width
                        height: 28
                        visible: root.recordPlaybackMode
                        spacing: 5

                        Repeater {
                            model: [
                                { "label": "0.5×", "speed": 0.5 },
                                { "label": "1×",   "speed": 1.0 },
                                { "label": "2×",   "speed": 2.0 },
                                { "label": "4×",   "speed": 4.0 }
                            ]

                            Rectangle {
                                width: (playbackSpeedRow.width - playbackSpeedRow.spacing * 3) / 4
                                height: playbackSpeedRow.height
                                radius: 6
                                color: Math.abs(recordReceiver.playbackSpeed - modelData.speed) < 0.01
                                       ? root.accent
                                       : speedBtnMouse.pressed ? "#29323a"
                                       : speedBtnMouse.containsMouse ? "#222a31" : "#101419"
                                border.color: Math.abs(recordReceiver.playbackSpeed - modelData.speed) < 0.01
                                              ? root.accent : "#313c46"

                                Text {
                                    anchors.centerIn: parent
                                    text: modelData.label
                                    color: Math.abs(recordReceiver.playbackSpeed - modelData.speed) < 0.01
                                           ? "#071014" : "#d8e2ea"
                                    font.pixelSize: 12
                                    font.bold: true
                                }

                                MouseArea {
                                    id: speedBtnMouse
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: recordReceiver.setPlaybackSpeed(modelData.speed)
                                }
                            }
                        }
                    }

                    // 回放文件信息
                    Text {
                        width: parent.width
                        visible: root.recordPlaybackMode && recordReceiver.playbackDurationSeconds > 0
                        text: root.mhzText(recordReceiver.playbackCenterFrequencyHz, 3)
                              + " MHz · " + root.sampleRateText(recordReceiver.playbackSampleRateHz)
                              + " · " + root.durationText(recordReceiver.playbackDurationSeconds)
                        color: "#788895"
                        font.pixelSize: 12
                        elide: Text.ElideRight
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

                // ---- Scanner controls ----
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
        id: fmComponent
        SpectrumView {
            accent: root.accent
            receiver: fmReceiver
            title: "FM FFT 信号频谱 / 能量强度"
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
        Item {
            // 回放代理：将 recordReceiver 的回放属性/信号包装为 WaterfallView 所需接口
            QtObject {
                id: playbackProxy
                property bool   running:           recordReceiver.playbackRunning
                property double centerFrequencyHz: recordReceiver.playbackCenterFrequencyHz
                property double sampleRateHz:      recordReceiver.playbackSampleRateHz
                property int    fftSize:           recordReceiver.fftSize
                signal spectrumReady(var levels, double leftFrequencyHz, double rightFrequencyHz)
            }

            Connections {
                target: recordReceiver
                onSpectrumReady: playbackProxy.spectrumReady(levels, leftFrequencyHz, rightFrequencyHz)
            }

            // ---- 录制统计视图（录制模式）----
            Rectangle {
                anchors.fill: parent
                visible: !root.recordPlaybackMode
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

                }
            }

            // ---- 回放瀑布图视图（回放模式）----
            WaterfallView {
                anchors.fill: parent
                visible: root.recordPlaybackMode
                accent: root.accent
                receiver: playbackProxy
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
