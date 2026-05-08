import QtQuick 2.12

Rectangle {
    id: root

    property color accent: "#2dd4bf"
    property var receiver: null
    property bool compactUi: width <= 520 || height <= 380

    radius: 8
    color: "#0b1115"
    border.color: "#2b3640"
    clip: true

    function mhzText(hz) {
        return (hz / 1000000).toFixed(3)
    }

    // Category color mapping
    function categoryColor(cat) {
        switch (cat) {
        case "tpms":    return "#f59e0b"   // amber  – tire pressure
        case "meter":   return "#60a5fa"   // blue   – utility meter
        case "car_key": return "#a78bfa"   // purple – car remote
        case "sensor":  return "#34d399"   // green  – temp/humidity
        default:        return "#94a3b8"   // slate  – generic
        }
    }

    // Category label
    function categoryLabel(cat) {
        switch (cat) {
        case "tpms":    return "胎压"
        case "meter":   return "仪表"
        case "car_key": return "车钥匙"
        case "sensor":  return "传感器"
        default:        return "通用"
        }
    }

    Column {
        id: overviewColumn
        anchors.left: parent.left
        anchors.leftMargin: root.compactUi ? 18 : 34
        anchors.right: parent.right
        anchors.rightMargin: root.compactUi ? 18 : 34
        anchors.top: parent.top
        anchors.topMargin: root.compactUi ? 16 : 30
        spacing: root.compactUi ? 10 : 18

        // ── Frequency + signal row ──────────────────────────────────────
        Row {
            width: parent.width
            height: root.compactUi ? 58 : 72
            spacing: root.compactUi ? 10 : 18

            Column {
                width: parent.width - signalPanel.width - parent.spacing
                height: parent.height
                spacing: root.compactUi ? 4 : 7

                Text {
                    width: parent.width
                    text: receiver ? root.mhzText(receiver.centerFrequencyHz) + " MHz" : ""
                    color: "#f6f8fb"
                    font.pixelSize: root.compactUi ? 32 : 42
                    font.bold: true
                    elide: Text.ElideRight
                }

                Text {
                    width: parent.width
                    text: receiver ? receiver.bandLabel : ""
                    color: receiver && receiver.running ? root.accent : "#8fa0ad"
                    font.pixelSize: root.compactUi ? 13 : 15
                    font.bold: true
                    elide: Text.ElideRight
                }
            }

            Rectangle {
                id: signalPanel
                width: root.compactUi ? 150 : 202
                height: parent.height
                radius: 8
                color: "#101820"
                border.color: "#29343d"

                Text {
                    anchors.left: parent.left
                    anchors.leftMargin: 14
                    anchors.right: parent.right
                    anchors.rightMargin: 14
                    anchors.top: parent.top
                    anchors.topMargin: 12
                    text: "信号"
                    color: "#8293a0"
                    font.pixelSize: root.compactUi ? 11 : 12
                    elide: Text.ElideRight
                }

                Row {
                    id: signalBars
                    anchors.left: parent.left
                    anchors.leftMargin: 14
                    anchors.right: parent.right
                    anchors.rightMargin: 14
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: root.compactUi ? 10 : 13
                    height: root.compactUi ? 22 : 28
                    spacing: root.compactUi ? 4 : 5

                    Repeater {
                        model: 14

                        Rectangle {
                            width: (signalBars.width - signalBars.spacing * 13) / 14
                            height: signalBars.height
                            radius: 4
                            color: receiver && index < Math.round(receiver.signalLevel * 14) ? root.accent : "#22303a"
                            opacity: receiver && index < Math.round(receiver.signalLevel * 14) ? 1.0 : 0.75
                        }
                    }
                }
            }
        }

        // ── Band quick-select buttons ───────────────────────────────────
        Row {
            width: parent.width
            height: root.compactUi ? 30 : 36
            spacing: root.compactUi ? 8 : 10

            Repeater {
                model: [
                    { "label": "433 MHz", "band": "433" },
                    { "label": "315 MHz", "band": "315" },
                    { "label": "303 MHz", "band": "303" }
                ]

                Rectangle {
                    width: root.compactUi ? 80 : 90
                    height: parent.height
                    radius: 6
                    color: receiver && receiver.bandLabel.indexOf(modelData.label.split(" ")[0]) >= 0
                           ? root.accent : "#1a2530"
                    border.color: "#2b3a47"

                    Text {
                        anchors.centerIn: parent
                        text: modelData.label
                        color: receiver && receiver.bandLabel.indexOf(modelData.label.split(" ")[0]) >= 0
                               ? "#0b1115" : "#8fa0ad"
                        font.pixelSize: 13
                        font.bold: true
                    }

                    MouseArea {
                        anchors.fill: parent
                        onClicked: { if (receiver) receiver.setBand(modelData.band) }
                    }
                }
            }

            Item { width: 1; height: 1 }   // spacer

            // Clear button
            Rectangle {
                width: root.compactUi ? 60 : 72
                height: parent.height
                radius: 6
                color: "#1a2530"
                border.color: "#2b3a47"

                Text {
                    anchors.centerIn: parent
                    text: "清空"
                    color: "#8fa0ad"
                    font.pixelSize: 13
                }

                MouseArea {
                    anchors.fill: parent
                    onClicked: { if (receiver) receiver.clearEvents() }
                }
            }
        }

        // ── Counter row ─────────────────────────────────────────────────
        Row {
            width: parent.width
            height: root.compactUi ? 54 : 70
            spacing: root.compactUi ? 10 : 14

            Repeater {
                model: [
                    { "label": "已识别设备", "value": receiver ? receiver.sensorCount : 0 },
                    { "label": "有效报文",   "value": receiver ? receiver.decodedMessages : 0 },
                    { "label": "候选帧",     "value": receiver ? receiver.totalFrames : 0 }
                ]

                Rectangle {
                    width: (parent.width - parent.spacing * 2) / 3
                    height: parent.height
                    radius: 8
                    color: "#101820"
                    border.color: "#29343d"

                    Text {
                        anchors.left: parent.left
                        anchors.leftMargin: 14
                        anchors.right: parent.right
                        anchors.rightMargin: 14
                        anchors.top: parent.top
                        anchors.topMargin: 10
                        text: modelData.label
                        color: "#8293a0"
                        font.pixelSize: 12
                        elide: Text.ElideRight
                    }

                    Text {
                        anchors.left: parent.left
                        anchors.leftMargin: 14
                        anchors.right: parent.right
                        anchors.rightMargin: 14
                        anchors.bottom: parent.bottom
                        anchors.bottomMargin: 10
                        text: modelData.value
                        color: "#eef6fb"
                        font.pixelSize: root.compactUi ? 20 : 25
                        font.bold: true
                        elide: Text.ElideRight
                    }
                }
            }
        }
    }

    // ── Section title ───────────────────────────────────────────────────
    Text {
        id: listTitle
        anchors.left: overviewColumn.left
        anchors.right: overviewColumn.right
        anchors.top: overviewColumn.bottom
        anchors.topMargin: root.compactUi ? 10 : 14
        text: "最近报文"
        color: "#eef6fb"
        font.pixelSize: root.compactUi ? 16 : 18
        font.bold: true
        elide: Text.ElideRight
    }

    // ── Event list ──────────────────────────────────────────────────────
    ListView {
        id: eventList
        anchors.left: listTitle.left
        anchors.right: listTitle.right
        anchors.top: listTitle.bottom
        anchors.topMargin: root.compactUi ? 8 : 12
        anchors.bottom: parent.bottom
        anchors.bottomMargin: root.compactUi ? 14 : 24
        model: receiver ? receiver.eventModel : null
        spacing: root.compactUi ? 6 : 10
        clip: true

        delegate: Rectangle {
            id: card
            width: ListView.view.width
            // Height adapts to content: TPMS and meter cards are taller
            height: root.compactUi ? ((category === "tpms" || category === "meter") ? 84 : 64) : ((category === "tpms" || category === "meter") ? 96 : 72)
            radius: 8
            color: "#101820"
            border.color: Qt.rgba(
                Qt.lighter(root.categoryColor(category), 1.0).r,
                Qt.lighter(root.categoryColor(category), 1.0).g,
                Qt.lighter(root.categoryColor(category), 1.0).b,
                0.35)

            // Left accent bar
            Rectangle {
                width: 4
                height: parent.height - 16
                anchors.left: parent.left
                anchors.leftMargin: 0
                anchors.verticalCenter: parent.verticalCenter
                radius: 2
                color: root.categoryColor(category)
            }

            // Category badge
            Rectangle {
                id: badge
                anchors.right: countLabel.left
                anchors.rightMargin: 8
                anchors.verticalCenter: modelLabel.verticalCenter
                width: badgeText.implicitWidth + 12
                height: 18
                radius: 9
                color: Qt.rgba(
                    root.categoryColor(category).r,
                    root.categoryColor(category).g,
                    root.categoryColor(category).b,
                    0.18)

                Text {
                    id: badgeText
                    anchors.centerIn: parent
                    text: root.categoryLabel(category)
                    color: root.categoryColor(category)
                    font.pixelSize: 10
                    font.bold: true
                }
            }

            // Model name
            Text {
                id: modelLabel
                anchors.left: parent.left
                anchors.leftMargin: 18
                anchors.right: badge.left
                anchors.rightMargin: 8
                anchors.top: parent.top
                anchors.topMargin: 10
                text: modelName
                color: "#eef6fb"
                font.pixelSize: 14
                font.bold: true
                elide: Text.ElideRight
            }

            // Event count + age
            Text {
                id: countLabel
                anchors.right: parent.right
                anchors.rightMargin: 14
                anchors.verticalCenter: modelLabel.verticalCenter
                text: "×" + eventCount + "  " + lastSeenSeconds + "s"
                color: "#7f93a2"
                font.pixelSize: 11
            }

            // ── TPMS specific row ──────────────────────────────────────
            Row {
                visible: category === "tpms"
                anchors.left: modelLabel.left
                anchors.right: parent.right
                anchors.rightMargin: 14
                anchors.top: modelLabel.bottom
                anchors.topMargin: 6
                spacing: 16

                Text {
                    text: pressureKPa.toFixed(1) + " kPa"
                    color: lowPressure ? "#f87171" : "#fbbf24"
                    font.pixelSize: 20
                    font.bold: true
                }

                Text {
                    text: pressurePSI.toFixed(1) + " PSI"
                    color: lowPressure ? "#f87171" : "#94a3b8"
                    font.pixelSize: 14
                    anchors.verticalCenter: parent.children[0].verticalCenter
                }

                Text {
                    text: pressureBar.toFixed(2) + " bar"
                    color: "#64748b"
                    font.pixelSize: 12
                    anchors.verticalCenter: parent.children[0].verticalCenter
                }

                Text {
                    visible: hasTemperature
                    text: temperatureC.toFixed(0) + "°C"
                    color: "#94a3b8"
                    font.pixelSize: 13
                    anchors.verticalCenter: parent.children[0].verticalCenter
                }

                Text {
                    visible: lowPressure
                    text: "⚠ 低压报警"
                    color: "#f87171"
                    font.pixelSize: 12
                    font.bold: true
                    anchors.verticalCenter: parent.children[0].verticalCenter
                }
            }

            // ── Meter specific row ─────────────────────────────────────
            Row {
                visible: category === "meter"
                anchors.left: modelLabel.left
                anchors.right: parent.right
                anchors.rightMargin: 14
                anchors.top: modelLabel.bottom
                anchors.topMargin: 6
                spacing: 10

                Text {
                    text: meterTypeName
                    color: "#60a5fa"
                    font.pixelSize: 13
                    font.bold: true
                    anchors.verticalCenter: parent.children[1].verticalCenter
                }

                Text {
                    text: meterConsumption.toFixed(3) + " " + meterUnit
                    color: "#e2e8f0"
                    font.pixelSize: 20
                    font.bold: true
                }
            }

            // ── Generic summary row ────────────────────────────────────
            Text {
                visible: category !== "tpms" && category !== "meter"
                anchors.left: modelLabel.left
                anchors.right: parent.right
                anchors.rightMargin: 14
                anchors.top: modelLabel.bottom
                anchors.topMargin: 7
                text: summary
                color: "#a8bac6"
                font.pixelSize: 12
                elide: Text.ElideRight
            }

            // ── Protocol / raw footer ──────────────────────────────────
            Text {
                anchors.left: modelLabel.left
                anchors.right: parent.right
                anchors.rightMargin: 14
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 8
                text: protocol + " / " + bitCount + " bit / " + raw
                color: "#4a5568"
                font.pixelSize: 10
                elide: Text.ElideMiddle
            }
        }

        // Empty state
        Text {
            anchors.centerIn: parent
            visible: parent.count === 0
            text: receiver && receiver.running ? "等待报文…" : "未启动"
            color: "#748796"
            font.pixelSize: 15
        }
    }
}