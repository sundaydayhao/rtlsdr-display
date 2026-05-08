import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Item {
    id: root

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 15

        // 标题栏
        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            Label {
                text: "LoRa 数据包嗅探"
                font.pixelSize: 24
                font.bold: true
            }

            Item { Layout.fillWidth: true }

            Label {
                text: loraReceiver.isRunning ? "● 运行中" : "○ 已停止"
                color: loraReceiver.isRunning ? "#4CAF50" : "#757575"
                font.pixelSize: 14
            }
        }

        // 控制面板
        Rectangle {
            Layout.fillWidth: true
            height: 180
            color: "#f5f5f5"
            radius: 8
            border.color: "#e0e0e0"
            border.width: 1

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 15
                spacing: 10

                // 频段选择
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10

                    Label {
                        text: "频段:"
                        font.pixelSize: 14
                        Layout.preferredWidth: 80
                    }

                    ButtonGroup {
                        id: bandGroup
                    }

                    RadioButton {
                        text: "433 MHz"
                        checked: true
                        ButtonGroup.group: bandGroup
                        onCheckedChanged: {
                            if (checked) loraReceiver.band = "433"
                        }
                    }

                    RadioButton {
                        text: "868 MHz"
                        ButtonGroup.group: bandGroup
                        onCheckedChanged: {
                            if (checked) loraReceiver.band = "868"
                        }
                    }

                    RadioButton {
                        text: "915 MHz"
                        ButtonGroup.group: bandGroup
                        onCheckedChanged: {
                            if (checked) loraReceiver.band = "915"
                        }
                    }

                    Item { Layout.fillWidth: true }
                }

                // 带宽选择
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10

                    Label {
                        text: "带宽:"
                        font.pixelSize: 14
                        Layout.preferredWidth: 80
                    }

                    ComboBox {
                        id: bandwidthCombo
                        Layout.preferredWidth: 150
                        model: ["125 kHz", "250 kHz", "500 kHz"]
                        currentIndex: 0
                        onCurrentIndexChanged: {
                            var bw = [125000, 250000, 500000][currentIndex]
                            loraReceiver.bandwidth = bw
                        }
                    }

                    Label {
                        text: "扩频因子:"
                        font.pixelSize: 14
                        Layout.leftMargin: 20
                    }

                    SpinBox {
                        id: sfSpinBox
                        from: 7
                        to: 12
                        value: 7
                        Layout.preferredWidth: 120
                        onValueChanged: {
                            loraReceiver.spreadingFactor = value
                        }
                    }

                    Item { Layout.fillWidth: true }
                }

                // 频率显示
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10

                    Label {
                        text: "中心频率:"
                        font.pixelSize: 14
                        Layout.preferredWidth: 80
                    }

                    Label {
                        text: (loraReceiver.frequency / 1000000).toFixed(3) + " MHz"
                        font.pixelSize: 14
                        font.bold: true
                        color: "#2196F3"
                    }

                    Item { Layout.fillWidth: true }
                }

                // 控制按钮
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10

                    Button {
                        text: loraReceiver.isRunning ? "停止" : "开始"
                        Layout.preferredWidth: 100
                        highlighted: true
                        onClicked: {
                            if (loraReceiver.isRunning) {
                                loraReceiver.stop()
                            } else {
                                loraReceiver.start()
                            }
                        }
                    }

                    Button {
                        text: "清除"
                        Layout.preferredWidth: 100
                        onClicked: {
                            loraReceiver.clearPackets()
                        }
                    }

                    Item { Layout.fillWidth: true }

                    Label {
                        text: "数据包: " + loraReceiver.packetModel.count
                        font.pixelSize: 14
                        color: "#666"
                    }
                }
            }
        }

        // 数据包列表
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: "#ffffff"
            radius: 8
            border.color: "#e0e0e0"
            border.width: 1

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 10
                spacing: 5

                // 表头
                Rectangle {
                    Layout.fillWidth: true
                    height: 35
                    color: "#f5f5f5"
                    radius: 4

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 10
                        anchors.rightMargin: 10
                        spacing: 10

                        Label {
                            text: "时间"
                            font.pixelSize: 12
                            font.bold: true
                            Layout.preferredWidth: 150
                        }

                        Label {
                            text: "数据 (HEX)"
                            font.pixelSize: 12
                            font.bold: true
                            Layout.fillWidth: true
                        }

                        Label {
                            text: "RSSI"
                            font.pixelSize: 12
                            font.bold: true
                            Layout.preferredWidth: 60
                        }

                        Label {
                            text: "SNR"
                            font.pixelSize: 12
                            font.bold: true
                            Layout.preferredWidth: 60
                        }

                        Label {
                            text: "SF"
                            font.pixelSize: 12
                            font.bold: true
                            Layout.preferredWidth: 40
                        }
                    }
                }

                // 数据列表
                ListView {
                    id: packetListView
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    model: loraReceiver.packetModel

                    ScrollBar.vertical: ScrollBar {
                        policy: ScrollBar.AsNeeded
                    }

                    delegate: Rectangle {
                        width: packetListView.width
                        height: 50
                        color: index % 2 === 0 ? "#ffffff" : "#fafafa"

                        Rectangle {
                            anchors.bottom: parent.bottom
                            width: parent.width
                            height: 1
                            color: "#e0e0e0"
                        }

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 10
                            anchors.rightMargin: 10
                            spacing: 10

                            Label {
                                text: model.timestamp
                                font.pixelSize: 11
                                font.family: "Consolas"
                                Layout.preferredWidth: 150
                            }

                            Label {
                                text: model.data
                                font.pixelSize: 11
                                font.family: "Consolas"
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                                color: model.crcValid ? "#000000" : "#f44336"
                            }

                            Label {
                                text: model.rssi + " dBm"
                                font.pixelSize: 11
                                font.family: "Consolas"
                                Layout.preferredWidth: 60
                                color: {
                                    if (model.rssi > -80) return "#4CAF50"
                                    if (model.rssi > -100) return "#FF9800"
                                    return "#f44336"
                                }
                            }

                            Label {
                                text: model.snr.toFixed(1) + " dB"
                                font.pixelSize: 11
                                font.family: "Consolas"
                                Layout.preferredWidth: 60
                                color: model.snr > 0 ? "#4CAF50" : "#FF9800"
                            }

                            Label {
                                text: "SF" + model.spreadingFactor
                                font.pixelSize: 11
                                font.family: "Consolas"
                                Layout.preferredWidth: 40
                            }
                        }

                        MouseArea {
                            anchors.fill: parent
                            onClicked: {
                                packetListView.currentIndex = index
                            }
                        }
                    }

                    // 空状态提示
                    Label {
                        anchors.centerIn: parent
                        text: "暂无数据包\n点击"开始"按钮开始监听"
                        font.pixelSize: 14
                        color: "#999"
                        horizontalAlignment: Text.AlignHCenter
                        visible: packetListView.count === 0
                    }
                }
            }
        }

        // 信息栏
        Rectangle {
            Layout.fillWidth: true
            height: 40
            color: "#f5f5f5"
            radius: 4

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 15
                anchors.rightMargin: 15
                spacing: 20

                Label {
                    text: "ℹ️ LoRaWAN 数据包嗅探工具"
                    font.pixelSize: 12
                    color: "#666"
                }

                Item { Layout.fillWidth: true }

                Label {
                    text: "支持频段: 433/868/915 MHz"
                    font.pixelSize: 11
                    color: "#999"
                }
            }
        }
    }

    // 连接信号
    Connections {
        target: loraReceiver
        function onPacketReceived(data, rssi, snr) {
            // 自动滚动到最新
            packetListView.positionViewAtEnd()
        }
        function onErrorOccurred(error) {
            console.error("LoRa Error:", error)
        }
    }
}