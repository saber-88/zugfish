import QtQuick
import QtQuick.Shapes

// Circular accuracy gauge with an animated sweep and count-up.
Item {
    id: root
    property real value: 0              // 0..100
    property real size: 88
    property string label
    property string sublabel
    property real shown: 0

    implicitWidth: size
    implicitHeight: size + 38
    onValueChanged: shown = value
    Component.onCompleted: shown = value
    Behavior on shown { NumberAnimation { duration: 900; easing.type: Easing.OutCubic } }

    readonly property color tint: Theme.winColor(shown)

    Item {
        id: ring
        width: root.size; height: root.size
        anchors.horizontalCenter: parent.horizontalCenter
        Shape {
            anchors.fill: parent
            ShapePath {
                strokeColor: Theme.bg4; strokeWidth: 7; fillColor: "transparent"
                PathAngleArc { centerX: ring.width / 2; centerY: ring.height / 2
                    radiusX: ring.width / 2 - 6; radiusY: ring.height / 2 - 6; startAngle: -90; sweepAngle: 360 }
            }
            ShapePath {
                strokeColor: root.tint; strokeWidth: 7; fillColor: "transparent"; capStyle: ShapePath.RoundCap
                PathAngleArc { centerX: ring.width / 2; centerY: ring.height / 2
                    radiusX: ring.width / 2 - 6; radiusY: ring.height / 2 - 6
                    startAngle: -90; sweepAngle: Math.max(0.1, 359.9 * root.shown / 100) }
            }
        }
        Text {
            anchors.centerIn: parent
            text: root.shown.toFixed(1)
            color: Theme.text0
            font.family: Theme.mono
            font.weight: Font.Medium
            font.pixelSize: Math.round(root.size * 0.25)
        }
    }
    Column {
        anchors.top: ring.bottom
        anchors.topMargin: 8
        anchors.horizontalCenter: parent.horizontalCenter
        spacing: 1
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: root.label
            width: Math.min(implicitWidth, root.size + 36)
            elide: Text.ElideRight
            horizontalAlignment: Text.AlignHCenter
            color: Theme.text0
            font.family: Theme.sans; font.weight: Font.DemiBold; font.pixelSize: Theme.fsMd
        }
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: root.sublabel
            color: Theme.text2
            font.family: Theme.sans; font.pixelSize: Theme.fsXs + 1
        }
    }
}
