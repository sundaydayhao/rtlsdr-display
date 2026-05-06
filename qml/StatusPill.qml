import QtQuick 2.12

Rectangle {
    id: root

    property string label
    property color accent: "#4cc9f0"

    width: Math.max(92, labelText.paintedWidth + 34)
    height: 30
    radius: 8
    color: Qt.rgba(accent.r, accent.g, accent.b, 0.12)
    border.color: Qt.rgba(accent.r, accent.g, accent.b, 0.42)

    Rectangle {
        anchors.left: parent.left
        anchors.leftMargin: 10
        anchors.verticalCenter: parent.verticalCenter
        width: 7
        height: 7
        radius: 4
        color: root.accent
    }

    Text {
        id: labelText
        anchors.left: parent.left
        anchors.leftMargin: 24
        anchors.right: parent.right
        anchors.rightMargin: 10
        anchors.verticalCenter: parent.verticalCenter
        text: root.label
        color: "#dce5ed"
        font.pixelSize: 12
        elide: Text.ElideRight
    }
}
