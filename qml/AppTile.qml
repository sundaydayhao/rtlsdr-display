import QtQuick 2.12

Item {
    id: root

    property string appId
    property string title
    property string subtitle
    property string iconKind
    property color accent: "#4cc9f0"

    signal activated(string appId)

    Rectangle {
        id: card
        anchors.fill: parent
        radius: 8
        color: mouseArea.pressed ? "#222a31" : mouseArea.containsMouse ? "#1c2329" : "#171c21"
        border.color: mouseArea.containsMouse ? root.accent : "#2a343d"
        border.width: 1
    }

    Rectangle {
        id: iconPlate
        anchors.left: parent.left
        anchors.leftMargin: 18
        anchors.top: parent.top
        anchors.topMargin: 18
        width: 58
        height: 58
        radius: 8
        color: Qt.rgba(root.accent.r, root.accent.g, root.accent.b, 0.15)
        border.color: Qt.rgba(root.accent.r, root.accent.g, root.accent.b, 0.45)

        IconGlyph {
            anchors.centerIn: parent
            width: 38
            height: 38
            kind: root.iconKind
            accent: root.accent
        }
    }

    Text {
        id: titleLabel
        anchors.left: parent.left
        anchors.leftMargin: 18
        anchors.right: parent.right
        anchors.rightMargin: 18
        anchors.top: iconPlate.bottom
        anchors.topMargin: 18
        text: root.title
        color: "#f5f7fa"
        font.pixelSize: 19
        font.bold: true
        wrapMode: Text.Wrap
        maximumLineCount: 2
        elide: Text.ElideRight
    }

    Text {
        anchors.left: titleLabel.left
        anchors.right: titleLabel.right
        anchors.top: titleLabel.bottom
        anchors.topMargin: 8
        text: root.subtitle
        color: "#96a6b4"
        font.pixelSize: 13
        wrapMode: Text.Wrap
        maximumLineCount: 2
        elide: Text.ElideRight
    }

    Rectangle {
        anchors.right: parent.right
        anchors.rightMargin: 16
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 14
        width: 34
        height: 24
        radius: 6
        color: "#0f1317"
        border.color: "#303b44"

        Canvas {
            anchors.centerIn: parent
            width: 14
            height: 14
            onPaint: {
                var ctx = getContext("2d")
                ctx.clearRect(0, 0, width, height)
                ctx.strokeStyle = "#d9e2ea"
                ctx.lineWidth = 2
                ctx.lineCap = "round"
                ctx.lineJoin = "round"
                ctx.beginPath()
                ctx.moveTo(4, 2)
                ctx.lineTo(10, 7)
                ctx.lineTo(4, 12)
                ctx.stroke()
            }
        }
    }

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: root.activated(root.appId)
    }
}
