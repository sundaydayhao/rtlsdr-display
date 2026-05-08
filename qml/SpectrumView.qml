import QtQuick 2.12

Rectangle {
    id: root

    property color accent: "#80ed99"
    property var receiver: null
    property string title: "FFT 信号频谱"
    property var levels: []
    property double leftFrequencyHz: receiver ? receiver.centerFrequencyHz - receiver.sampleRateHz / 2 : 0
    property double rightFrequencyHz: receiver ? receiver.centerFrequencyHz + receiver.sampleRateHz / 2 : 0
    property double averageLevel: 0
    property double peakLevel: 0
    property double peakFrequencyHz: receiver ? receiver.centerFrequencyHz : 0
    property double peakToAverageDb: 0
    property bool compactUi: width <= 640 || height <= 380

    radius: 8
    color: "#090d12"
    border.color: "#2b3640"
    clip: true

    onWidthChanged: spectrumCanvas.requestPaint()
    onHeightChanged: spectrumCanvas.requestPaint()
    onAccentChanged: spectrumCanvas.requestPaint()
    onLevelsChanged: spectrumCanvas.requestPaint()

    Connections {
        target: root.receiver

        onSpectrumReady: {
            root.pushSpectrum(levels, leftFrequencyHz, rightFrequencyHz)
        }

        onCenterFrequencyHzChanged: spectrumCanvas.requestPaint()
        onSampleRateHzChanged: spectrumCanvas.requestPaint()
        onRunningChanged: {
            if (!root.receiver || !root.receiver.running) {
                root.levels = []
                root.averageLevel = 0
                root.peakLevel = 0
                root.peakToAverageDb = 0
            }
            spectrumCanvas.requestPaint()
        }
    }

    function pushSpectrum(incomingLevels, leftHz, rightHz) {
        if (!incomingLevels || incomingLevels.length === 0) {
            return
        }

        var next = []
        var sum = 0
        var peak = 0
        var peakIndex = 0
        for (var i = 0; i < incomingLevels.length; ++i) {
            var value = Math.max(0, Math.min(1, Number(incomingLevels[i])))
            next.push(value)
            sum += value
            if (value > peak) {
                peak = value
                peakIndex = i
            }
        }

        var average = sum / Math.max(1, next.length)
        root.leftFrequencyHz = leftHz
        root.rightFrequencyHz = rightHz
        root.averageLevel = average
        root.peakLevel = peak
        root.peakFrequencyHz = leftHz + (rightHz - leftHz) * ((peakIndex + 0.5) / Math.max(1, next.length))
        root.peakToAverageDb = 20 * Math.log((peak + 0.001) / (average + 0.001)) / Math.LN10
        root.levels = next
    }

    function frequencyLabel(hz) {
        if (Math.abs(hz) >= 1000000000) {
            return (hz / 1000000000).toFixed(4) + " GHz"
        }
        if (Math.abs(hz) >= 1000000) {
            return (hz / 1000000).toFixed(3) + " MHz"
        }
        if (Math.abs(hz) >= 1000) {
            return (hz / 1000).toFixed(1) + " kHz"
        }
        return hz.toFixed(0) + " Hz"
    }

    function percentText(value) {
        return Math.round(Math.max(0, Math.min(1, value)) * 100) + "%"
    }

    Canvas {
        id: spectrumCanvas
        anchors.fill: parent
        antialiasing: true

        onPaint: {
            var ctx = getContext("2d")
            ctx.clearRect(0, 0, width, height)
            ctx.fillStyle = "#090d12"
            ctx.fillRect(0, 0, width, height)

            var headerH = root.compactUi ? 78 : 92
            var footerH = root.compactUi ? 30 : 36
            var plotLeft = 18
            var plotRight = width - 18
            var plotTop = headerH
            var plotBottom = height - footerH
            var plotW = Math.max(1, plotRight - plotLeft)
            var plotH = Math.max(1, plotBottom - plotTop)

            ctx.fillStyle = "#0e151b"
            ctx.fillRect(0, 0, width, headerH)
            ctx.fillStyle = "rgba(9, 13, 18, 0.88)"
            ctx.fillRect(0, height - footerH, width, footerH)

            ctx.strokeStyle = "#1f2c36"
            ctx.lineWidth = 1
            for (var gx = 0; gx <= 8; ++gx) {
                var x = Math.round(plotLeft + gx * plotW / 8) + 0.5
                ctx.beginPath()
                ctx.moveTo(x, plotTop)
                ctx.lineTo(x, plotBottom)
                ctx.stroke()
            }
            for (var gy = 0; gy <= 4; ++gy) {
                var y = Math.round(plotTop + gy * plotH / 4) + 0.5
                ctx.beginPath()
                ctx.moveTo(plotLeft, y)
                ctx.lineTo(plotRight, y)
                ctx.stroke()
            }

            if (root.levels.length > 0) {
                var gradient = ctx.createLinearGradient(0, plotTop, 0, plotBottom)
                gradient.addColorStop(0, String(root.accent))
                gradient.addColorStop(1, "rgba(128, 237, 153, 0.10)")

                ctx.beginPath()
                ctx.moveTo(plotLeft, plotBottom)
                for (var i = 0; i < root.levels.length; ++i) {
                    var px = plotLeft + i * plotW / Math.max(1, root.levels.length - 1)
                    var py = plotBottom - root.levels[i] * plotH
                    ctx.lineTo(px, py)
                }
                ctx.lineTo(plotRight, plotBottom)
                ctx.closePath()
                ctx.fillStyle = gradient
                ctx.globalAlpha = 0.28
                ctx.fill()
                ctx.globalAlpha = 1.0

                ctx.strokeStyle = String(root.accent)
                ctx.lineWidth = 2
                ctx.beginPath()
                for (var j = 0; j < root.levels.length; ++j) {
                    var sx = plotLeft + j * plotW / Math.max(1, root.levels.length - 1)
                    var sy = plotBottom - root.levels[j] * plotH
                    if (j === 0) {
                        ctx.moveTo(sx, sy)
                    } else {
                        ctx.lineTo(sx, sy)
                    }
                }
                ctx.stroke()

                var peakRatio = (root.peakFrequencyHz - root.leftFrequencyHz) / Math.max(1, root.rightFrequencyHz - root.leftFrequencyHz)
                var peakX = plotLeft + Math.max(0, Math.min(1, peakRatio)) * plotW
                ctx.strokeStyle = "#eef6fb"
                ctx.lineWidth = 1
                ctx.beginPath()
                ctx.moveTo(Math.round(peakX) + 0.5, plotTop)
                ctx.lineTo(Math.round(peakX) + 0.5, plotBottom)
                ctx.stroke()
            } else {
                ctx.fillStyle = "#6f8190"
                ctx.font = "16px sans-serif"
                ctx.textAlign = "center"
                ctx.textBaseline = "middle"
                ctx.fillText(root.receiver && root.receiver.running ? "等待 FFT 数据..." : "点击启动开始采集 FFT 频谱", width / 2, plotTop + plotH / 2)
            }

            ctx.fillStyle = "#9fb2c0"
            ctx.font = "12px sans-serif"
            ctx.textBaseline = "middle"
            var footerY = height - footerH / 2
            ctx.textAlign = "left"
            ctx.fillText(root.frequencyLabel(root.leftFrequencyHz), plotLeft, footerY)
            ctx.textAlign = "center"
            ctx.fillText(root.frequencyLabel((root.leftFrequencyHz + root.rightFrequencyHz) / 2), width / 2, footerY)
            ctx.textAlign = "right"
            ctx.fillText(root.frequencyLabel(root.rightFrequencyHz), plotRight, footerY)
        }
    }

    Text {
        anchors.left: parent.left
        anchors.leftMargin: 18
        anchors.top: parent.top
        anchors.topMargin: 14
        text: root.title
        color: "#eef6fb"
        font.pixelSize: 18
        font.bold: true
    }

    Row {
        anchors.left: parent.left
        anchors.leftMargin: 18
        anchors.right: parent.right
        anchors.rightMargin: 18
        anchors.top: parent.top
        anchors.topMargin: root.compactUi ? 46 : 52
        height: root.compactUi ? 22 : 26
        spacing: root.compactUi ? 6 : 10

        Repeater {
            model: root.compactUi ? [
                { "label": "平均", "value": root.percentText(root.averageLevel) },
                { "label": "峰值", "value": root.percentText(root.peakLevel) },
                { "label": "音频", "value": receiver ? root.percentText(receiver.audioLevel) : "0%" }
            ] : [
                { "label": "平均能量", "value": root.percentText(root.averageLevel) },
                { "label": "峰值强度", "value": root.percentText(root.peakLevel) },
                { "label": "峰值频率", "value": root.frequencyLabel(root.peakFrequencyHz) },
                { "label": "峰均差", "value": root.peakToAverageDb.toFixed(1) + " dB" },
                { "label": "音频电平", "value": receiver ? root.percentText(receiver.audioLevel) : "0%" }
            ]

            Rectangle {
                width: root.compactUi ? (parent.width - parent.spacing * 2) / 3 : (parent.width - parent.spacing * 4) / 5
                height: parent.height
                radius: 7
                color: "#11181e"
                border.color: "#29343d"

                Text {
                    anchors.left: parent.left
                    anchors.leftMargin: 9
                    anchors.verticalCenter: parent.verticalCenter
                    text: modelData.label + "："
                    color: "#7f91a0"
                    font.pixelSize: 11
                }

                Text {
                    anchors.right: parent.right
                    anchors.rightMargin: 9
                    anchors.verticalCenter: parent.verticalCenter
                    text: modelData.value
                    color: "#eef6fb"
                    font.pixelSize: 12
                    font.bold: true
                }
            }
        }
    }
}
