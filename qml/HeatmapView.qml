import QtQuick 2.12

Rectangle {
    id: root

    property color accent: "#a855f7"
    property var receiver: null
    property string title: "宽带频谱扫描 / 热力图"
    property bool markerTracksFrequency: false
    property double trackedFrequencyHz: 0

    signal frequencySelected(double frequencyHz)

    // Internal state
    property var rows: []
    property int maxRows: Math.max(32, Math.floor((height - 76) / 4))
    property double latestStartHz: receiver ? receiver.startFrequencyHz : 0
    property double latestStopHz:  receiver ? receiver.stopFrequencyHz  : 0
    property bool   hasLiveSpan:   false
    property bool   hasMarker:     false
    property double markerFrequencyHz: 0

    radius: 8
    color: "#090d12"
    border.color: "#2b3640"
    clip: true

    onWidthChanged:  heatmapCanvas.requestPaint()
    onHeightChanged: heatmapCanvas.requestPaint()
    onAccentChanged: heatmapCanvas.requestPaint()
    onHasMarkerChanged: heatmapCanvas.requestPaint()
    onMarkerFrequencyHzChanged: heatmapCanvas.requestPaint()
    onTitleChanged: heatmapCanvas.requestPaint()
    onTrackedFrequencyHzChanged: syncTrackedMarker()
    onMarkerTracksFrequencyChanged: syncTrackedMarker()

    Component.onCompleted: syncTrackedMarker()

    // -----------------------------------------------------------------------
    // Signal connections
    // -----------------------------------------------------------------------
    Connections {
        target: root.receiver

        onScanLineReady: {
            root.pushLine(powers, startHz, stopHz)
        }

        onStartFrequencyHzChanged: {
            root.rows = []
            root.hasLiveSpan = false
            root.hasMarker = false
            root.syncTrackedMarker()
            heatmapCanvas.requestPaint()
        }

        onStopFrequencyHzChanged: {
            root.rows = []
            root.hasLiveSpan = false
            root.hasMarker = false
            root.syncTrackedMarker()
            heatmapCanvas.requestPaint()
        }

        onRunningChanged: {
            if (root.receiver && !root.receiver.running) {
                heatmapCanvas.requestPaint()
            }
        }
    }

    // -----------------------------------------------------------------------
    // Helper functions
    // -----------------------------------------------------------------------
    function pushLine(powers, startHz, stopHz) {
        if (!powers || powers.length === 0) {
            return
        }

        // Downsample to at most 1024 display bins for rendering performance
        var displayBins = Math.min(powers.length, 1024)
        var row = []

        if (powers.length <= displayBins) {
            for (var i = 0; i < powers.length; ++i) {
                row.push(Number(powers[i]))
            }
        } else {
            var ratio = powers.length / displayBins
            for (var i = 0; i < displayBins; ++i) {
                var binStart = Math.floor(i * ratio)
                var binEnd   = Math.min(powers.length, Math.floor((i + 1) * ratio))
                var sum = 0
                var cnt = Math.max(1, binEnd - binStart)
                for (var j = binStart; j < binEnd; ++j) {
                    sum += Number(powers[j])
                }
                row.push(sum / cnt)
            }
        }

        root.latestStartHz = startHz
        root.latestStopHz  = stopHz
        root.hasLiveSpan   = true

        var next = root.rows.slice(0)
        next.unshift(row)
        if (next.length > root.maxRows) {
            next.length = root.maxRows
        }
        root.rows = next
        heatmapCanvas.requestPaint()
    }

    function frequencyLabel(hz) {
        if (Math.abs(hz) >= 1000000000) {
            return (hz / 1000000000).toFixed(3) + " GHz"
        }
        if (Math.abs(hz) >= 10000000) {
            return (hz / 1000000).toFixed(1) + " MHz"
        }
        if (Math.abs(hz) >= 1000000) {
            return (hz / 1000000).toFixed(3) + " MHz"
        }
        if (Math.abs(hz) >= 1000) {
            return (hz / 1000).toFixed(0) + " kHz"
        }
        return hz.toFixed(0) + " Hz"
    }

    function displayStartHz() {
        return root.hasLiveSpan ? root.latestStartHz
                                : (root.receiver ? root.receiver.startFrequencyHz : 0)
    }

    function displayStopHz() {
        return root.hasLiveSpan ? root.latestStopHz
                                : (root.receiver ? root.receiver.stopFrequencyHz : 0)
    }

    function frequencyAtX(pixelX) {
        var startHz = root.displayStartHz()
        var stopHz = root.displayStopHz()
        var spanHz = stopHz - startHz
        if (!isFinite(spanHz) || spanHz <= 0 || root.width <= 0) {
            return startHz
        }

        var ratio = Math.max(0, Math.min(1, pixelX / root.width))
        return startHz + spanHz * ratio
    }

    function markerPixelX() {
        var startHz = root.displayStartHz()
        var stopHz = root.displayStopHz()
        var spanHz = stopHz - startHz
        if (!isFinite(spanHz) || spanHz <= 0 || root.width <= 0) {
            return -1
        }
        return (root.markerFrequencyHz - startHz) / spanHz * root.width
    }

    function setMarker(frequencyHz) {
        root.markerFrequencyHz = frequencyHz
        root.hasMarker = true
        heatmapCanvas.requestPaint()
    }

    function syncTrackedMarker() {
        if (root.markerTracksFrequency) {
            root.setMarker(root.trackedFrequencyHz)
        }
    }

    // -----------------------------------------------------------------------
    // Canvas
    // -----------------------------------------------------------------------
    Canvas {
        id: heatmapCanvas
        anchors.fill: parent
        antialiasing: false

        onPaint: {
            var ctx = getContext("2d")
            ctx.clearRect(0, 0, width, height)

            // Layout
            var headerH = 50
            var footerH = 26
            var plotTop    = headerH
            var plotHeight = Math.max(1, height - headerH - footerH)

            // Background
            ctx.fillStyle = "#090d12"
            ctx.fillRect(0, 0, width, height)

            // Vertical grid lines (8 divisions)
            ctx.strokeStyle = "#1a2530"
            ctx.lineWidth = 1
            for (var gx = 1; gx < 8; ++gx) {
                var px = Math.round(gx * width / 8) + 0.5
                ctx.beginPath()
                ctx.moveTo(px, plotTop)
                ctx.lineTo(px, plotTop + plotHeight)
                ctx.stroke()
            }

            // ---- Draw heatmap rows ----
            if (root.rows.length > 0) {
                var numRows = root.rows.length
                var cellH   = plotHeight / Math.max(1, root.maxRows)

                for (var y = 0; y < numRows; ++y) {
                    var row    = root.rows[y]
                    var rowLen = row.length
                    if (rowLen === 0) continue

                    var cellW  = width / rowLen
                    var rowTop = plotTop + y * cellH

                    for (var x = 0; x < rowLen; ++x) {
                        ctx.fillStyle = colorFor(row[x])
                        ctx.fillRect(x * cellW, rowTop, Math.ceil(cellW), Math.ceil(cellH) + 1)
                    }
                }
            } else {
                // Placeholder when no data
                ctx.fillStyle = "#0e151b"
                ctx.fillRect(0, plotTop, width, plotHeight)

                ctx.fillStyle = "#3a4d5c"
                ctx.font = "14px sans-serif"
                ctx.textBaseline = "middle"
                ctx.textAlign = "center"
                ctx.fillText("等待扫描数据…", width / 2, plotTop + plotHeight / 2)
            }

            // ---- Header bar ----
            ctx.fillStyle = "rgba(9, 13, 18, 0.88)"
            ctx.fillRect(0, 0, width, headerH)

            // Color scale legend (right side of header)
            var legendW = 120
            var legendH = 12
            var legendX = width - legendW - 16
            var legendY = (headerH - legendH) / 2

            var grad = ctx.createLinearGradient(legendX, 0, legendX + legendW, 0)
            grad.addColorStop(0.00, "#000000")
            grad.addColorStop(0.15, "#0a0a5e")
            grad.addColorStop(0.30, "#0000ff")
            grad.addColorStop(0.45, "#00bfff")
            grad.addColorStop(0.60, "#00ff80")
            grad.addColorStop(0.75, "#ffff00")
            grad.addColorStop(0.88, "#ff8000")
            grad.addColorStop(1.00, "#ff0000")
            ctx.fillStyle = grad
            ctx.fillRect(legendX, legendY, legendW, legendH)
            ctx.strokeStyle = "#3a4d5c"
            ctx.lineWidth = 1
            ctx.strokeRect(legendX + 0.5, legendY + 0.5, legendW - 1, legendH - 1)

            ctx.fillStyle = "#7a8d9a"
            ctx.font = "10px sans-serif"
            ctx.textBaseline = "middle"
            ctx.textAlign = "left"
            ctx.fillText("弱", legendX - 18, legendY + legendH / 2)
            ctx.textAlign = "left"
            ctx.fillText("强", legendX + legendW + 4, legendY + legendH / 2)

            // ---- Footer bar with frequency labels ----
            ctx.fillStyle = "rgba(9, 13, 18, 0.88)"
            ctx.fillRect(0, height - footerH, width, footerH)

            var dispStart = root.displayStartHz()
            var dispStop  = root.displayStopHz()

            if (root.hasMarker) {
                var markerX = root.markerPixelX()
                if (markerX >= 0 && markerX <= width) {
                    var crispX = Math.round(markerX) + 0.5
                    var markerLabel = root.frequencyLabel(root.markerFrequencyHz)

                    ctx.strokeStyle = "#eef6fb"
                    ctx.lineWidth = 1
                    ctx.beginPath()
                    ctx.moveTo(crispX, plotTop)
                    ctx.lineTo(crispX, plotTop + plotHeight)
                    ctx.stroke()

                    ctx.font = "12px sans-serif"
                    var labelPaddingX = 9
                    var availableLabelWidth = Math.max(32, width - 16)
                    var labelWidth = Math.min(availableLabelWidth, Math.ceil(ctx.measureText(markerLabel).width) + labelPaddingX * 2)
                    var labelHeight = 24
                    var labelX = Math.max(8, Math.min(Math.max(8, width - labelWidth - 8), markerX - labelWidth / 2))
                    var labelY = Math.max(plotTop + 8, Math.min(plotTop + plotHeight - labelHeight - 8, plotTop + 10))
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
            var footerMidY = height - footerH / 2

            ctx.textAlign = "left"
            ctx.fillText(root.frequencyLabel(dispStart), 12, footerMidY)
            ctx.textAlign = "center"
            ctx.fillText(root.frequencyLabel((dispStart + dispStop) / 2), width / 2, footerMidY)
            ctx.textAlign = "right"
            ctx.fillText(root.frequencyLabel(dispStop), width - 12, footerMidY)

            // Separator lines
            ctx.strokeStyle = "#2b3640"
            ctx.lineWidth = 1
            ctx.beginPath()
            ctx.moveTo(0, headerH - 0.5)
            ctx.lineTo(width, headerH - 0.5)
            ctx.stroke()
            ctx.beginPath()
            ctx.moveTo(0, height - footerH + 0.5)
            ctx.lineTo(width, height - footerH + 0.5)
            ctx.stroke()
        }

        // Jet-style colormap (discrete steps for performance)
        function colorFor(v) {
            if (v < 0.08) return "#000000"
            if (v < 0.18) return "#0a0a5e"
            if (v < 0.30) return "#0000ff"
            if (v < 0.43) return "#00bfff"
            if (v < 0.57) return "#00ff80"
            if (v < 0.70) return "#80ff00"
            if (v < 0.82) return "#ffff00"
            if (v < 0.92) return "#ff8000"
            return "#ff0000"
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

    // -----------------------------------------------------------------------
    // Overlay labels
    // -----------------------------------------------------------------------
    Text {
        anchors.left: parent.left
        anchors.leftMargin: 18
        anchors.top: parent.top
        anchors.topMargin: 16
        text: root.title
        color: "#eef6fb"
        font.pixelSize: 17
        font.bold: true
    }

}
