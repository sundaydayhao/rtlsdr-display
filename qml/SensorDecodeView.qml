import QtQuick 2.12

Rectangle {
    id: root

    property color accent: "#2dd4bf"
    property var receiver: null

    radius: 8
    color: "#0b1115"
    border.color: "#2b3640"
    clip: true

    function mhzText(hz) {
        return (hz / 1000000).toFixed(3)
    }

    Column {
        anchors.left: parent.left
        anchors.leftMargin: 34
        anchors.right: parent.right
        anchors.rightMargin: 34
        anchors.top: parent.top
        anchors.topMargin: 30
        spacing: 18

        Row {
            width: parent.width
            height: 72
            spacing: 18

            Column {
                width: parent.width - 220
                height: parent.height
                spacing: 7

                Text {
                    width: parent.width
                    text: receiver ? root.mhzText(receiver.centerFrequencyHz) + " MHz" : ""
                    color: "#f6f8fb"
                    font.pixelSize: 42
                    font.bold: true
                    elide: Text.ElideRight
                }

                Text {
                    width: parent.width
                    text: receiver ? receiver.bandLabel + " / " + (receiver.running ? "解码中" : "停止") : ""
                    color: receiver && receiver.running ? root.accent : "#8fa0ad"
                    font.pixelSize: 15
                    font.bold: true
                    elide: Text.ElideRight
                }
            }

            Rectangle {
                width: 202
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
                    font.pixelSize: 12
                    elide: Text.ElideRight
                }

                Row {
                    id: signalBars
                    anchors.left: parent.left
                    anchors.leftMargin: 14
                    anchors.right: parent.right
                    anchors.rightMargin: 14
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: 13
                    height: 28
                    spacing: 5

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

        Row {
            width: parent.width
            height: 70
            spacing: 14

            Repeater {
                model: [
                    { "label": "传感器", "value": receiver ? receiver.sensorCount : 0 },
                    { "label": "有效报文", "value": receiver ? receiver.decodedMessages : 0 },
                    { "label": "候选帧", "value": receiver ? receiver.totalFrames : 0 }
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
                        font.pixelSize: 25
                        font.bold: true
                        elide: Text.ElideRight
                    }
                }
            }
        }
    }

    Text {
        id: listTitle
        anchors.left: parent.left
        anchors.leftMargin: 34
        anchors.right: parent.right
        anchors.rightMargin: 34
        anchors.top: parent.top
        anchors.topMargin: 210
        text: "最近报文"
        color: "#eef6fb"
        font.pixelSize: 18
        font.bold: true
        elide: Text.ElideRight
    }

    ListView {
        anchors.left: listTitle.left
        anchors.right: listTitle.right
        anchors.top: listTitle.bottom
        anchors.topMargin: 12
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 24
        model: receiver ? receiver.eventModel : null
        spacing: 10
        clip: true

        delegate: Rectangle {
            width: ListView.view.width
            height: 72
            radius: 8
            color: "#101820"
            border.color: "#29343d"

            Text {
                id: modelLabel
                anchors.left: parent.left
                anchors.leftMargin: 14
                anchors.right: countLabel.left
                anchors.rightMargin: 12
                anchors.top: parent.top
                anchors.topMargin: 10
                text: modelName
                color: "#eef6fb"
                font.pixelSize: 15
                font.bold: true
                elide: Text.ElideRight
            }

            Text {
                id: countLabel
                anchors.right: parent.right
                anchors.rightMargin: 14
                anchors.verticalCenter: modelLabel.verticalCenter
                text: "x" + eventCount + " · " + lastSeenSeconds + "s"
                color: "#7f93a2"
                font.pixelSize: 11
            }

            Text {
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

            Text {
                anchors.left: modelLabel.left
                anchors.right: parent.right
                anchors.rightMargin: 14
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 8
                text: protocol + " / " + bitCount + " bit / " + raw
                color: "#748796"
                font.pixelSize: 11
                elide: Text.ElideMiddle
            }
        }

        Text {
            anchors.centerIn: parent
            visible: parent.count === 0
            text: receiver && receiver.running ? "等待报文" : "未启动"
            color: "#748796"
            font.pixelSize: 15
        }
    }
}
