import QtQuick 2.12

Rectangle {
    id: root

    property color accent: "#f77f00"
    property var receiver: null
    property string title: "实时频谱 / 瀑布图"
    property var rows: []
    property int bins: receiver ? receiver.fftSize : 512
    property int maxRows: Math.max(28, Math.floor((height - 112) / 4))
    property bool hasLiveSpan: false
    property double latestLeftFrequencyHz: 0
    property double latestRightFrequencyHz: 0
    property bool hasIdleFrequencySpan: false
    property double idleLeftFrequencyHz: 0
    property double idleRightFrequencyHz: 0
    property double leftFrequencyHz: hasIdleFrequencySpan ? idleLeftFrequencyHz : (hasLiveSpan ? latestLeftFrequencyHz : (receiver ? receiver.centerFrequencyHz - receiver.sampleRateHz / 2 : 0))
    property double rightFrequencyHz: hasIdleFrequencySpan ? idleRightFrequencyHz : (hasLiveSpan ? latestRightFrequencyHz : (receiver ? receiver.centerFrequencyHz + receiver.sampleRateHz / 2 : 0))
    property bool hasMarker: false
    property double markerFrequencyHz: 0
    property bool markerTracksReceiver: false

    signal frequencySelected(double frequencyHz)

    radius: 8
    color: "#090d12"
    border.color: "#2b3640"
    clip: true

    onWidthChanged: waterfallCanvas.requestPaint()
    onHeightChanged: waterfallCanvas.requestPaint()
    onAccentChanged: waterfallCanvas.requestPaint()
    onHasMarkerChanged: waterfallCanvas.requestPaint()
    onMarkerFrequencyHzChanged: waterfallCanvas.requestPaint()
    onHasIdleFrequencySpanChanged: resetRows()
    onIdleLeftFrequencyHzChanged: resetRows()
    onIdleRightFrequencyHzChanged: resetRows()

    Component.onCompleted: syncMarkerToReceiver()

    onReceiverChanged: syncMarkerToReceiver()

    Connections {
        target: root.receiver

        onSpectrumReady: {
            root.pushSpectrum(levels, leftFrequencyHz, rightFrequencyHz)
        }

        onCenterFrequencyHzChanged: {
            if (!root.receiver || !root.receiver.running) {
                root.hasLiveSpan = false
            }
            root.syncMarkerToReceiver()
            waterfallCanvas.requestPaint()
        }
        onSampleRateHzChanged: {
            root.hasLiveSpan = false
            waterfallCanvas.requestPaint()
        }
    }

    function pushSpectrum(levels, leftHz, rightHz) {
        if (!levels || levels.length === 0) {
            return
        }

        var incoming = []
        for (var i = 0; i < levels.length; ++i) {
            incoming.push(Number(levels[i]))
        }

        var row = incoming
        if (root.hasIdleFrequencySpan) {
            var displaySpan = root.idleRightFrequencyHz - root.idleLeftFrequencyHz
            var liveSpan = rightHz - leftHz
            var targetBins = Math.max(128, root.bins > 0 ? root.bins : incoming.length)
            var sums = []
            var counts = []
            row = []
            for (var b = 0; b < targetBins; ++b) {
                sums.push(0)
                counts.push(0)
                row.push(0)
            }

            if (isFinite(displaySpan) && displaySpan > 0 && isFinite(liveSpan) && liveSpan > 0) {
                for (var j = 0; j < incoming.length; ++j) {
                    var frequencyHz = leftHz + liveSpan * ((j + 0.5) / incoming.length)
                    var displayIndex = Math.floor((frequencyHz - root.idleLeftFrequencyHz) / displaySpan * targetBins)
                    if (displayIndex >= 0 && displayIndex < targetBins) {
                        sums[displayIndex] += incoming[j]
                        counts[displayIndex] += 1
                    }
                }
                for (var k = 0; k < targetBins; ++k) {
                    if (counts[k] > 0) {
                        row[k] = sums[k] / counts[k]
                    }
                }
            }
        }

        root.bins = row.length
        root.latestLeftFrequencyHz = leftHz
        root.latestRightFrequencyHz = rightHz
        root.hasLiveSpan = true

        var next = root.rows.slice(0)
        next.unshift(row)
        if (next.length > root.maxRows) {
            next.length = root.maxRows
        }
        root.rows = next
        waterfallCanvas.requestPaint()
    }

    function resetRows() {
        root.rows = []
        root.hasLiveSpan = false
        waterfallCanvas.requestPaint()
    }

    function setMarker(frequencyHz) {
        root.markerFrequencyHz = frequencyHz
        root.hasMarker = true
        waterfallCanvas.requestPaint()
    }

    function syncMarkerToReceiver() {
        if (root.markerTracksReceiver && root.receiver) {
            root.setMarker(root.receiver.centerFrequencyHz)
        }
    }

    function frequencyLabel(hz) {
        if (Math.abs(hz) >= 1000000000) {
            return (hz / 1000000000).toFixed(4) + " GHz"
        }
        return (hz / 1000000).toFixed(3) + " MHz"
    }

    function frequencyAtX(pixelX) {
        var spanHz = root.rightFrequencyHz - root.leftFrequencyHz
        if (!isFinite(spanHz) || spanHz <= 0 || root.width <= 0) {
            return root.leftFrequencyHz
        }

        var ratio = Math.max(0, Math.min(1, pixelX / root.width))
        return root.leftFrequencyHz + spanHz * ratio
    }

    function markerPixelX() {
        var spanHz = root.rightFrequencyHz - root.leftFrequencyHz
        if (!isFinite(spanHz) || spanHz <= 0 || root.width <= 0) {
            return -1
        }
        return (root.markerFrequencyHz - root.leftFrequencyHz) / spanHz * root.width
    }

    Canvas {
        id: waterfallCanvas
        anchors.fill: parent
        antialiasing: false

        onPaint: {
            var ctx = getContext("2d")
            ctx.clearRect(0, 0, width, height)
            ctx.fillStyle = "#090d12"
            ctx.fillRect(0, 0, width, height)

            var spectrumHeight = Math.min(112, Math.max(88, height * 0.25))
            var plotTop = spectrumHeight
            var plotHeight = Math.max(1, height - spectrumHeight)
            var cellW = width / Math.max(1, root.bins)
            var cellH = Math.max(2, plotHeight / Math.max(1, root.maxRows))

            ctx.fillStyle = "#0e151b"
            ctx.fillRect(0, 0, width, spectrumHeight)

            ctx.strokeStyle = "#263541"
            ctx.lineWidth = 1
            for (var gx = 0; gx <= 8; ++gx) {
                var px = gx * width / 8
                ctx.beginPath()
                ctx.moveTo(px, 0)
                ctx.lineTo(px, height)
                ctx.stroke()
            }

            ctx.strokeStyle = "#1b2730"
            for (var gy = 1; gy <= 3; ++gy) {
                var py = gy * spectrumHeight / 4
                ctx.beginPath()
                ctx.moveTo(0, py)
                ctx.lineTo(width, py)
                ctx.stroke()
            }

            for (var y = 0; y < root.rows.length; ++y) {
                var row = root.rows[y]
                for (var x = 0; x < row.length; ++x) {
                    ctx.fillStyle = colorFor(row[x])
                    ctx.fillRect(x * cellW, plotTop + y * cellH, Math.ceil(cellW), Math.ceil(cellH))
                }
            }

            var latest = root.rows.length > 0 ? root.rows[0] : []
            if (latest.length > 0) {
                ctx.strokeStyle = String(root.accent)
                ctx.lineWidth = 2
                ctx.beginPath()
                for (var i = 0; i < latest.length; ++i) {
                    var sx = i * cellW
                    var sy = spectrumHeight - 18 - latest[i] * (spectrumHeight - 42)
                    if (i === 0) {
                        ctx.moveTo(sx, sy)
                    } else {
                        ctx.lineTo(sx, sy)
                    }
                }
                ctx.stroke()
            }

            ctx.fillStyle = "rgba(9, 13, 18, 0.72)"
            ctx.fillRect(0, spectrumHeight - 22, width, 22)

            if (root.hasMarker) {
                var markerX = root.markerPixelX()
                if (markerX >= 0 && markerX <= width) {
                    var crispX = Math.round(markerX) + 0.5
                    var markerLabel = root.frequencyLabel(root.markerFrequencyHz)
                    ctx.strokeStyle = "#eef6fb"
                    ctx.lineWidth = 1
                    ctx.beginPath()
                    ctx.moveTo(crispX, 0)
                    ctx.lineTo(crispX, height)
                    ctx.stroke()

                    ctx.font = "12px sans-serif"
                    var labelPaddingX = 9
                    var availableLabelWidth = Math.max(32, width - 16)
                    var labelWidth = Math.min(availableLabelWidth, Math.ceil(ctx.measureText(markerLabel).width) + labelPaddingX * 2)
                    var labelHeight = 24
                    var labelX = Math.max(8, Math.min(Math.max(8, width - labelWidth - 8), markerX - labelWidth / 2))
                    var labelY = Math.max(8, Math.min(height - labelHeight - 8, plotTop + 10))
                    ctx.fillStyle = "rgba(9, 13, 18, 0.88)"
                    ctx.fillRect(labelX, labelY, labelWidth, labelHeight)
                    ctx.strokeStyle = String(root.accent)
                    ctx.strokeRect(labelX + 0.5, labelY + 0.5, labelWidth - 1, labelHeight - 1)
                    ctx.fillStyle = "#eef6fb"
                    ctx.textBaseline = "middle"
                    ctx.textAlign = "center"
                    ctx.fillText(markerLabel, labelX + labelWidth / 2, labelY + labelHeight / 2, labelWidth - labelPaddingX * 2)
                }
            }

            ctx.fillStyle = "#9fb2c0"
            ctx.font = "12px sans-serif"
            ctx.textBaseline = "middle"
            ctx.textAlign = "left"
            ctx.fillText(root.frequencyLabel(root.leftFrequencyHz), 12, spectrumHeight - 11)
            ctx.textAlign = "center"
            ctx.fillText(root.frequencyLabel((root.leftFrequencyHz + root.rightFrequencyHz) / 2), width / 2, spectrumHeight - 11)
            ctx.textAlign = "right"
            ctx.fillText(root.frequencyLabel(root.rightFrequencyHz), width - 12, spectrumHeight - 11)
        }

        function colorFor(v) {
            if (v < 0.15) {
                return "#06101c"
            }
            if (v < 0.32) {
                return "#0a355e"
            }
            if (v < 0.50) {
                return "#0c7d89"
            }
            if (v < 0.68) {
                return "#35b779"
            }
            if (v < 0.84) {
                return "#f4d35e"
            }
            return "#f77f00"
        }
    }

    MouseArea {
        anchors.fill: parent
        cursorShape: Qt.CrossCursor

        onClicked: {
            var frequencyHz = root.frequencyAtX(mouse.x)
            root.setMarker(frequencyHz)
            root.frequencySelected(frequencyHz)
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

    Text {
        anchors.right: parent.right
        anchors.rightMargin: 18
        anchors.top: parent.top
        anchors.topMargin: 17
        text: receiver ? (receiver.running ? "接收中" : "停止") : ""
        color: receiver && receiver.running ? "#80ed99" : "#9fb2c0"
        font.pixelSize: 12
        font.bold: true
    }
}
