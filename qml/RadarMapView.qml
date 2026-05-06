import QtQuick 2.12

Rectangle {
    id: root

    property color accent: "#4cc9f0"
    property var aircraftModel: null
    property int aircraftCount: 0
    property real centerLatitude: 31.2304
    property real centerLongitude: 121.4737
    property string mapProvider: "osm"
    property string tileUrlTemplate: "https://tile.openstreetmap.org/{z}/{x}/{y}.png"
    property string mapAttribution: "© OpenStreetMap contributors"
    property int zoomLevel: 9
    property int minimumZoom: 3
    property int maximumZoom: 17

    property real mapLatitude: centerLatitude
    property real mapLongitude: centerLongitude
    property int tileSize: 256
    property var visibleTiles: []
    property var selectedAircraft: null

    readonly property real maxMercatorLatitude: 85.05112878

    onCenterLatitudeChanged: {
        mapLatitude = clampLatitude(centerLatitude)
        refreshTiles()
    }
    onCenterLongitudeChanged: {
        mapLongitude = normalizeLongitude(centerLongitude)
        refreshTiles()
    }
    onMapLatitudeChanged: refreshTiles()
    onMapLongitudeChanged: refreshTiles()
    onZoomLevelChanged: refreshTiles()
    onWidthChanged: refreshTiles()
    onHeightChanged: refreshTiles()
    Component.onCompleted: refreshTiles()

    radius: 8
    color: "#0d1317"
    border.color: "#2b3640"
    clip: true

    function clampLatitude(latitude) {
        return Math.max(-maxMercatorLatitude, Math.min(maxMercatorLatitude, latitude))
    }

    function normalizeLongitude(longitude) {
        var lon = longitude
        while (lon < -180) {
            lon += 360
        }
        while (lon >= 180) {
            lon -= 360
        }
        return lon
    }

    function mapScale(zoom) {
        return tileSize * Math.pow(2, zoom)
    }

    function lonToWorldX(longitude, zoom) {
        return (normalizeLongitude(longitude) + 180) / 360 * mapScale(zoom)
    }

    function latToWorldY(latitude, zoom) {
        var lat = clampLatitude(latitude)
        var sinLatitude = Math.sin(lat * Math.PI / 180)
        return (0.5 - Math.log((1 + sinLatitude) / (1 - sinLatitude)) / (4 * Math.PI)) * mapScale(zoom)
    }

    function worldXToLon(worldX, zoom) {
        var scale = mapScale(zoom)
        var x = ((worldX % scale) + scale) % scale
        return x / scale * 360 - 180
    }

    function worldYToLat(worldY, zoom) {
        var scale = mapScale(zoom)
        var y = Math.max(0, Math.min(scale, worldY))
        var n = Math.PI - 2 * Math.PI * y / scale
        return 180 / Math.PI * Math.atan(0.5 * (Math.exp(n) - Math.exp(-n)))
    }

    function coordinateToPoint(latitude, longitude) {
        var centerX = lonToWorldX(mapLongitude, zoomLevel)
        var centerY = latToWorldY(mapLatitude, zoomLevel)
        return Qt.point(width * 0.5 + lonToWorldX(longitude, zoomLevel) - centerX,
                        height * 0.5 + latToWorldY(latitude, zoomLevel) - centerY)
    }

    function setCenterFromWorld(worldX, worldY, zoom) {
        mapLatitude = clampLatitude(worldYToLat(worldY, zoom))
        mapLongitude = normalizeLongitude(worldXToLon(worldX, zoom))
    }

    function panBy(deltaX, deltaY) {
        var centerX = lonToWorldX(mapLongitude, zoomLevel)
        var centerY = latToWorldY(mapLatitude, zoomLevel)
        setCenterFromWorld(centerX + deltaX, centerY + deltaY, zoomLevel)
    }

    function zoomAt(pointX, pointY, delta) {
        var nextZoom = Math.max(minimumZoom, Math.min(maximumZoom, zoomLevel + delta))
        if (nextZoom === zoomLevel) {
            return
        }

        var oldCenterX = lonToWorldX(mapLongitude, zoomLevel)
        var oldCenterY = latToWorldY(mapLatitude, zoomLevel)
        var anchorLon = worldXToLon(oldCenterX + pointX - width * 0.5, zoomLevel)
        var anchorLat = worldYToLat(oldCenterY + pointY - height * 0.5, zoomLevel)

        zoomLevel = nextZoom

        var newAnchorX = lonToWorldX(anchorLon, zoomLevel)
        var newAnchorY = latToWorldY(anchorLat, zoomLevel)
        setCenterFromWorld(newAnchorX - pointX + width * 0.5,
                           newAnchorY - pointY + height * 0.5,
                           zoomLevel)
    }

    function buildTileUrl(zoom, tileX, tileY) {
        var tilesPerAxis = Math.pow(2, zoom)
        var wrappedX = ((tileX % tilesPerAxis) + tilesPerAxis) % tilesPerAxis
        return tileUrlTemplate.replace("{z}", zoom)
                              .replace("{x}", wrappedX)
                              .replace("{y}", tileY)
    }

    function refreshTiles() {
        if (width <= 0 || height <= 0) {
            visibleTiles = []
            return
        }

        var centerX = lonToWorldX(mapLongitude, zoomLevel)
        var centerY = latToWorldY(mapLatitude, zoomLevel)
        var startX = Math.floor((centerX - width * 0.5) / tileSize) - 1
        var endX = Math.floor((centerX + width * 0.5) / tileSize) + 1
        var startY = Math.floor((centerY - height * 0.5) / tileSize) - 1
        var endY = Math.floor((centerY + height * 0.5) / tileSize) + 1
        var tilesPerAxis = Math.pow(2, zoomLevel)
        var tiles = []

        for (var y = startY; y <= endY; ++y) {
            if (y < 0 || y >= tilesPerAxis) {
                continue
            }

            for (var x = startX; x <= endX; ++x) {
                tiles.push({
                    "key": zoomLevel + ":" + x + ":" + y,
                    "screenX": width * 0.5 + x * tileSize - centerX,
                    "screenY": height * 0.5 + y * tileSize - centerY,
                    "url": buildTileUrl(zoomLevel, x, y)
                })
            }
        }

        visibleTiles = tiles
    }

    function aircraftTitle(aircraft) {
        if (!aircraft) {
            return ""
        }
        return aircraft.callsign && aircraft.callsign.length > 0 ? aircraft.callsign : aircraft.icao
    }

    function altitudeText(hasAltitude, altitude) {
        return hasAltitude ? altitude + " ft" : "高度未知"
    }

    function speedText(hasSpeed, speed) {
        return hasSpeed ? Math.round(speed) + " kt" : "速度未知"
    }

    function headingText(hasHeading, heading) {
        return hasHeading ? Math.round(heading) + "°" : "航向未知"
    }

    function positionText(latitude, longitude) {
        return latitude.toFixed(5) + ", " + longitude.toFixed(5)
    }

    Rectangle {
        anchors.fill: parent
        color: "#182128"
    }

    Repeater {
        model: root.visibleTiles

        Image {
            x: modelData.screenX
            y: modelData.screenY
            width: root.tileSize + 1
            height: root.tileSize + 1
            source: modelData.url
            asynchronous: true
            cache: true
            fillMode: Image.PreserveAspectCrop
            sourceSize.width: root.tileSize
            sourceSize.height: root.tileSize
            opacity: status === Image.Ready ? 1 : 0

            Behavior on opacity {
                NumberAnimation {
                    duration: 140
                }
            }
        }
    }

    Rectangle {
        anchors.fill: parent
        color: "#061016"
        opacity: 0.12
    }

    Canvas {
        id: centerCrosshair
        anchors.centerIn: parent
        width: 34
        height: 34
        opacity: 0.48

        onPaint: {
            var ctx = getContext("2d")
            ctx.clearRect(0, 0, width, height)
            ctx.strokeStyle = String(root.accent)
            ctx.lineWidth = 1.4
            ctx.beginPath()
            ctx.moveTo(width * 0.5, 4)
            ctx.lineTo(width * 0.5, height - 4)
            ctx.moveTo(4, height * 0.5)
            ctx.lineTo(width - 4, height * 0.5)
            ctx.stroke()
        }
    }

    MouseArea {
        id: mapMouse
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton
        hoverEnabled: true
        property real lastX: 0
        property real lastY: 0
        property real pressedX: 0
        property real pressedY: 0
        property bool dragging: false

        onPressed: {
            lastX = mouse.x
            lastY = mouse.y
            pressedX = mouse.x
            pressedY = mouse.y
            dragging = false
        }

        onPositionChanged: {
            if (!pressed) {
                return
            }
            var dx = lastX - mouse.x
            var dy = lastY - mouse.y
            if (Math.abs(mouse.x - pressedX) > 3 || Math.abs(mouse.y - pressedY) > 3) {
                dragging = true
            }
            panBy(dx, dy)
            lastX = mouse.x
            lastY = mouse.y
        }

        onClicked: {
            if (!dragging) {
                root.selectedAircraft = null
            }
        }

        onWheel: {
            zoomAt(wheel.x, wheel.y, wheel.angleDelta.y > 0 ? 1 : -1)
        }
    }

    Repeater {
        model: root.aircraftModel

        Item {
            id: aircraftMarker

            property point screenPoint: root.coordinateToPoint(latitude, longitude)
            property bool onScreen: screenPoint.x > -180 && screenPoint.x < root.width + 180
                                    && screenPoint.y > -100 && screenPoint.y < root.height + 100

            width: Math.min(190, Math.max(128, labelBox.width + 42))
            height: 70
            visible: hasPosition && onScreen
            x: screenPoint.x - 15
            y: screenPoint.y - 15
            z: 30

            Canvas {
                id: planeIcon
                width: 30
                height: 30
                rotation: hasHeading ? heading : 0
                antialiasing: true

                onPaint: {
                    var ctx = getContext("2d")
                    ctx.clearRect(0, 0, width, height)
                    ctx.shadowColor = "rgba(0, 0, 0, 0.42)"
                    ctx.shadowBlur = 6
                    ctx.fillStyle = String(root.accent)
                    ctx.beginPath()
                    ctx.moveTo(width * 0.5, 1)
                    ctx.lineTo(width * 0.73, height * 0.88)
                    ctx.lineTo(width * 0.50, height * 0.72)
                    ctx.lineTo(width * 0.27, height * 0.88)
                    ctx.closePath()
                    ctx.fill()
                    ctx.shadowBlur = 0
                    ctx.strokeStyle = "#05202b"
                    ctx.lineWidth = 1.2
                    ctx.stroke()
                }
            }

            Rectangle {
                id: labelBox
                x: 36
                y: 0
                width: Math.min(148, Math.max(86, callsignText.paintedWidth + 20))
                height: 44
                radius: 6
                color: aircraftMouse.containsMouse ? "#101820" : "#081016"
                border.color: root.selectedAircraft && root.selectedAircraft.icao === icao ? root.accent : "#31414d"
                opacity: 0.94

                Text {
                    id: callsignText
                    anchors.left: parent.left
                    anchors.leftMargin: 10
                    anchors.right: parent.right
                    anchors.rightMargin: 10
                    anchors.top: parent.top
                    anchors.topMargin: 7
                    text: callsign.length > 0 ? callsign : icao
                    color: "#eef6fb"
                    font.pixelSize: 12
                    font.bold: true
                    elide: Text.ElideRight
                }

                Text {
                    anchors.left: callsignText.left
                    anchors.right: callsignText.right
                    anchors.top: callsignText.bottom
                    anchors.topMargin: 3
                    text: root.altitudeText(hasAltitude, altitude)
                    color: "#9fb2c0"
                    font.pixelSize: 11
                    elide: Text.ElideRight
                }
            }

            MouseArea {
                id: aircraftMouse
                anchors.left: planeIcon.left
                anchors.top: planeIcon.top
                width: labelBox.x + labelBox.width
                height: labelBox.height
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                propagateComposedEvents: false
                onClicked: {
                    root.selectedAircraft = {
                        "icao": icao,
                        "callsign": callsign,
                        "altitude": altitude,
                        "hasAltitude": hasAltitude,
                        "latitude": latitude,
                        "longitude": longitude,
                        "speed": speed,
                        "hasSpeed": hasSpeed,
                        "heading": heading,
                        "hasHeading": hasHeading,
                        "messages": messages,
                        "lastSeenSeconds": lastSeenSeconds
                    }
                }
            }
        }
    }

    Rectangle {
        id: titleBadge
        anchors.left: parent.left
        anchors.leftMargin: 16
        anchors.top: parent.top
        anchors.topMargin: 14
        width: Math.min(parent.width - 32, titleColumn.implicitWidth + 28)
        height: 64
        radius: 8
        color: "#081016"
        border.color: "#263640"
        opacity: 0.93

        Column {
            id: titleColumn
            anchors.left: parent.left
            anchors.leftMargin: 14
            anchors.verticalCenter: parent.verticalCenter
            spacing: 5

            Text {
                text: "ADS-B 实时地图"
                color: "#f4f8fb"
                font.pixelSize: 17
                font.bold: true
            }

            Text {
                text: "OSM · z" + root.zoomLevel + " · " + root.mapLatitude.toFixed(4) + ", " + root.mapLongitude.toFixed(4)
                color: "#94a7b6"
                font.pixelSize: 12
            }
        }
    }

    Column {
        id: zoomControls
        anchors.right: parent.right
        anchors.rightMargin: 16
        anchors.top: parent.top
        anchors.topMargin: 14
        spacing: 8
        z: 60

        Repeater {
            model: [
                { "label": "+", "delta": 1 },
                { "label": "-", "delta": -1 }
            ]

            Rectangle {
                width: 38
                height: 38
                radius: 7
                color: zoomMouse.pressed ? "#25313a" : zoomMouse.containsMouse ? "#1b2630" : "#081016"
                border.color: "#33444f"

                Text {
                    anchors.centerIn: parent
                    text: modelData.label
                    color: "#edf6fb"
                    font.pixelSize: 22
                    font.bold: true
                }

                MouseArea {
                    id: zoomMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.zoomAt(root.width * 0.5, root.height * 0.5, modelData.delta)
                }
            }
        }

        Rectangle {
            width: 38
            height: 38
            radius: 7
            color: recenterMouse.pressed ? "#25313a" : recenterMouse.containsMouse ? "#1b2630" : "#081016"
            border.color: "#33444f"

            Canvas {
                anchors.centerIn: parent
                width: 18
                height: 18

                onPaint: {
                    var ctx = getContext("2d")
                    ctx.clearRect(0, 0, width, height)
                    ctx.strokeStyle = "#edf6fb"
                    ctx.lineWidth = 1.8
                    ctx.beginPath()
                    ctx.arc(width * 0.5, height * 0.5, 6, 0, Math.PI * 2)
                    ctx.moveTo(width * 0.5, 0)
                    ctx.lineTo(width * 0.5, 4)
                    ctx.moveTo(width * 0.5, height - 4)
                    ctx.lineTo(width * 0.5, height)
                    ctx.moveTo(0, height * 0.5)
                    ctx.lineTo(4, height * 0.5)
                    ctx.moveTo(width - 4, height * 0.5)
                    ctx.lineTo(width, height * 0.5)
                    ctx.stroke()
                }
            }

            MouseArea {
                id: recenterMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: {
                    root.mapLatitude = root.clampLatitude(root.centerLatitude)
                    root.mapLongitude = root.normalizeLongitude(root.centerLongitude)
                    root.refreshTiles()
                }
            }
        }
    }

    Rectangle {
        id: aircraftDetails
        anchors.left: parent.left
        anchors.leftMargin: 16
        anchors.bottom: attribution.top
        anchors.bottomMargin: 10
        width: Math.min(330, parent.width - 32)
        height: 162
        radius: 8
        color: "#081016"
        border.color: "#2d3e49"
        opacity: 0.94
        visible: root.selectedAircraft !== null
        z: 70

        Text {
            id: detailTitle
            anchors.left: parent.left
            anchors.leftMargin: 16
            anchors.right: closeDetails.left
            anchors.rightMargin: 10
            anchors.top: parent.top
            anchors.topMargin: 13
            text: root.aircraftTitle(root.selectedAircraft)
            color: "#f3f8fb"
            font.pixelSize: 18
            font.bold: true
            elide: Text.ElideRight
        }

        Rectangle {
            id: closeDetails
            anchors.right: parent.right
            anchors.rightMargin: 10
            anchors.verticalCenter: detailTitle.verticalCenter
            width: 26
            height: 26
            radius: 6
            color: closeMouse.containsMouse ? "#1a2630" : "#0d171f"
            border.color: "#31414d"

            Text {
                anchors.centerIn: parent
                text: "×"
                color: "#dce8ef"
                font.pixelSize: 17
            }

            MouseArea {
                id: closeMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: root.selectedAircraft = null
            }
        }

        Column {
            anchors.left: detailTitle.left
            anchors.right: parent.right
            anchors.rightMargin: 16
            anchors.top: detailTitle.bottom
            anchors.topMargin: 12
            spacing: 8

            Text {
                width: parent.width
                text: root.selectedAircraft ? "ICAO " + root.selectedAircraft.icao : ""
                color: "#a8bac6"
                font.pixelSize: 12
                elide: Text.ElideRight
            }

            Text {
                width: parent.width
                text: root.selectedAircraft
                      ? root.altitudeText(root.selectedAircraft.hasAltitude, root.selectedAircraft.altitude)
                        + " · " + root.speedText(root.selectedAircraft.hasSpeed, root.selectedAircraft.speed)
                        + " · " + root.headingText(root.selectedAircraft.hasHeading, root.selectedAircraft.heading)
                      : ""
                color: "#dce8ef"
                font.pixelSize: 13
                elide: Text.ElideRight
            }

            Text {
                width: parent.width
                text: root.selectedAircraft ? root.positionText(root.selectedAircraft.latitude, root.selectedAircraft.longitude) : ""
                color: "#a8bac6"
                font.pixelSize: 12
                elide: Text.ElideRight
            }

            Text {
                width: parent.width
                text: root.selectedAircraft ? "消息 " + root.selectedAircraft.messages + " · " + root.selectedAircraft.lastSeenSeconds + " 秒前" : ""
                color: "#7f93a2"
                font.pixelSize: 12
                elide: Text.ElideRight
            }
        }
    }

    Rectangle {
        id: emptyState
        anchors.centerIn: parent
        width: Math.min(360, parent.width - 56)
        height: 66
        radius: 8
        color: "#081016"
        border.color: "#273741"
        opacity: 0.9
        visible: root.aircraftCount === 0

        Text {
            anchors.centerIn: parent
            width: parent.width - 32
            horizontalAlignment: Text.AlignHCenter
            text: "等待 1090 MHz ADS-B 信号"
            color: "#c6d3dc"
            font.pixelSize: 17
            elide: Text.ElideRight
        }
    }

    Text {
        id: attribution
        anchors.right: parent.right
        anchors.rightMargin: 12
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 8
        text: root.mapAttribution
        color: "#eef6fb"
        opacity: 0.74
        font.pixelSize: 11
        z: 80

        Rectangle {
            anchors.fill: parent
            anchors.margins: -5
            radius: 5
            color: "#081016"
            opacity: 0.76
            z: -1
        }
    }
}
