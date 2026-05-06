import QtQuick 2.12

Item {
    id: root

    property string kind: "plane"
    property color accent: "#4cc9f0"

    onKindChanged: canvas.requestPaint()
    onAccentChanged: canvas.requestPaint()

    Canvas {
        id: canvas
        anchors.fill: parent
        antialiasing: true

        onPaint: {
            var ctx = getContext("2d")
            var w = width
            var h = height
            var c = String(root.accent)
            ctx.clearRect(0, 0, w, h)
            ctx.strokeStyle = c
            ctx.fillStyle = c
            ctx.lineWidth = 2.4
            ctx.lineCap = "round"
            ctx.lineJoin = "round"

            if (root.kind === "plane") {
                ctx.save()
                ctx.translate(w * 0.5, h * 0.5)
                ctx.rotate(-0.35)
                ctx.beginPath()
                ctx.moveTo(0, -h * 0.42)
                ctx.lineTo(w * 0.13, h * 0.18)
                ctx.lineTo(w * 0.40, h * 0.34)
                ctx.lineTo(w * 0.10, h * 0.36)
                ctx.lineTo(0, h * 0.42)
                ctx.lineTo(-w * 0.10, h * 0.36)
                ctx.lineTo(-w * 0.40, h * 0.34)
                ctx.lineTo(-w * 0.13, h * 0.18)
                ctx.closePath()
                ctx.fill()
                ctx.restore()
                return
            }

            if (root.kind === "am" || root.kind === "fm") {
                ctx.beginPath()
                ctx.moveTo(w * 0.5, h * 0.74)
                ctx.lineTo(w * 0.5, h * 0.30)
                ctx.stroke()
                ctx.beginPath()
                ctx.arc(w * 0.5, h * 0.30, w * 0.08, 0, Math.PI * 2)
                ctx.fill()
                for (var i = 0; i < 3; ++i) {
                    var r = w * (0.18 + i * 0.12)
                    ctx.beginPath()
                    ctx.arc(w * 0.5, h * 0.30, r, -2.45, -0.70)
                    ctx.stroke()
                    ctx.beginPath()
                    ctx.arc(w * 0.5, h * 0.30, r, -0.44, 1.25)
                    ctx.stroke()
                }
                if (root.kind === "fm") {
                    ctx.beginPath()
                    ctx.moveTo(w * 0.18, h * 0.80)
                    ctx.bezierCurveTo(w * 0.34, h * 0.58, w * 0.48, h * 0.98, w * 0.64, h * 0.76)
                    ctx.bezierCurveTo(w * 0.72, h * 0.66, w * 0.78, h * 0.66, w * 0.86, h * 0.76)
                    ctx.stroke()
                }
                return
            }

            if (root.kind === "waterfall") {
                var colors = ["#152c45", "#175f8a", "#12a1a8", "#f4d35e", "#f77f00"]
                for (var y = 0; y < 7; ++y) {
                    for (var x = 0; x < 5; ++x) {
                        ctx.fillStyle = colors[(x + y) % colors.length]
                        ctx.globalAlpha = 0.55 + (y / 16)
                        ctx.fillRect(w * (0.08 + x * 0.18), h * (0.12 + y * 0.11), w * 0.13, h * 0.07)
                    }
                }
                ctx.globalAlpha = 1.0
                ctx.strokeStyle = c
                ctx.beginPath()
                ctx.moveTo(w * 0.08, h * 0.16)
                ctx.lineTo(w * 0.24, h * 0.16)
                ctx.lineTo(w * 0.36, h * 0.08)
                ctx.lineTo(w * 0.50, h * 0.26)
                ctx.lineTo(w * 0.66, h * 0.12)
                ctx.lineTo(w * 0.88, h * 0.16)
                ctx.stroke()
                return
            }

            if (root.kind === "sensor") {
                ctx.strokeStyle = c
                ctx.fillStyle = c
                ctx.lineWidth = 2.3
                ctx.beginPath()
                ctx.arc(w * 0.50, h * 0.62, w * 0.10, 0, Math.PI * 2)
                ctx.fill()
                ctx.beginPath()
                ctx.moveTo(w * 0.50, h * 0.72)
                ctx.lineTo(w * 0.50, h * 0.86)
                ctx.moveTo(w * 0.34, h * 0.86)
                ctx.lineTo(w * 0.66, h * 0.86)
                ctx.stroke()
                for (var ring = 0; ring < 3; ++ring) {
                    var radius = w * (0.18 + ring * 0.12)
                    ctx.beginPath()
                    ctx.arc(w * 0.50, h * 0.58, radius, -2.35, -0.78)
                    ctx.stroke()
                    ctx.beginPath()
                    ctx.arc(w * 0.50, h * 0.58, radius, -0.20, 1.38)
                    ctx.stroke()
                }
                ctx.globalAlpha = 0.88
                ctx.fillRect(w * 0.18, h * 0.14, w * 0.12, h * 0.12)
                ctx.fillRect(w * 0.70, h * 0.18, w * 0.12, h * 0.12)
                ctx.fillRect(w * 0.22, h * 0.38, w * 0.10, h * 0.10)
                ctx.globalAlpha = 1.0
                return
            }

            if (root.kind === "record") {
                ctx.beginPath()
                ctx.arc(w * 0.5, h * 0.5, Math.min(w, h) * 0.30, 0, Math.PI * 2)
                ctx.fill()
                ctx.strokeStyle = "#ffd7df"
                ctx.beginPath()
                ctx.moveTo(w * 0.10, h * 0.78)
                ctx.lineTo(w * 0.26, h * 0.78)
                ctx.lineTo(w * 0.32, h * 0.62)
                ctx.lineTo(w * 0.42, h * 0.88)
                ctx.lineTo(w * 0.52, h * 0.68)
                ctx.lineTo(w * 0.62, h * 0.78)
                ctx.lineTo(w * 0.90, h * 0.78)
                ctx.stroke()
            }
        }

    }
}
