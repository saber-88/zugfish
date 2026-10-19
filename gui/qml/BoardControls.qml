import QtQuick

// Transport controls under the board. Collapses to the essentials when the board is narrow.
Item {
    id: root
    implicitHeight: 40
    readonly property bool compact: width < 470

    component Divider: Row {
        spacing: 0
        Item { width: 14; height: 1 }
        Rectangle { width: 1; height: 20; color: Theme.line; anchors.verticalCenter: parent.verticalCenter }
        Item { width: 14; height: 1 }
    }

    Row {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.verticalCenter: parent.verticalCenter
        spacing: 2

        IconButton { icon: "chevrons-left"; tip: "Start  (Home)"; enabled: App.canPrev; onClicked: App.first() }
        IconButton { icon: "chevron-left"; tip: "Previous  (←)"; enabled: App.canPrev; onClicked: App.prev() }
        IconButton {
            primary: true
            icon: App.autoplay ? "pause" : "play"
            tip: App.autoplay ? "Pause  (Space)" : "Play  (Space)"
            onClicked: App.toggleAutoplay()
        }
        IconButton { icon: "chevron-right"; tip: "Next  (→)"; enabled: App.canNext; onClicked: App.next() }
        IconButton { icon: "chevrons-right"; tip: "End  (End)"; enabled: App.canNext; onClicked: App.last() }

        Divider { anchors.verticalCenter: parent.verticalCenter }

        IconButton { icon: "refresh-cw"; tip: "Flip board  (F)"; onClicked: App.toggleFlip() }
        IconButton {
            icon: "target"
            tip: App.showArrows ? "Hide arrows" : "Show arrows"
            active: App.showArrows
            onClicked: App.showArrows = !App.showArrows
        }

        Divider { visible: !root.compact; anchors.verticalCenter: parent.verticalCenter }

        AppButton {
            visible: !root.compact
            anchors.verticalCenter: parent.verticalCenter
            variant: "ghost"; compact: true
            icon: "chevron-left"; text: "Key"
            enabled: App.reviewState === 2
            onClicked: App.jumpKey(-1)
        }
        AppButton {
            visible: !root.compact
            anchors.verticalCenter: parent.verticalCenter
            variant: "ghost"; compact: true
            icon: "chevron-right"; iconAfter: true; text: "Key"
            enabled: App.reviewState === 2
            onClicked: App.jumpKey(1)
        }
        Item { visible: !root.compact; width: 10; height: 1 }
        Text {
            visible: !root.compact
            anchors.verticalCenter: parent.verticalCenter
            text: App.navigationPly + " / " + App.navigationCount
            color: Theme.text2
            font.family: Theme.mono
            font.pixelSize: Theme.fsSm + 1
        }
    }
}
