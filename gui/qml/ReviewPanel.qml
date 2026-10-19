import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// "Review" tab: run/progress, current move coach card, accuracy, key moments, breakdown.
Item {
    id: root

    readonly property var cur: App.current
    readonly property var sum: App.summary
    readonly property bool done: App.reviewState === 2

    Flickable {
        id: flick
        anchors.fill: parent
        contentWidth: width
        contentHeight: col.implicitHeight + 24
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ThemedScrollBar {}

        ColumnLayout {
            id: col
            x: 0; y: 0
            width: flick.width - 6
            spacing: 14

            // ---------- not reviewed yet / running / failed ----------
            Card {
                Layout.fillWidth: true
                visible: !root.done
                implicitHeight: runCol.implicitHeight + 32
                ColumnLayout {
                    id: runCol
                    anchors.fill: parent
                    anchors.margins: 16
                    spacing: 10
                    RowLayout {
                        spacing: 10
                        Rectangle {
                            width: 34; height: 34; radius: 10; color: Qt.rgba(0.55, 0.49, 1, 0.14)
                            Icon { anchors.centerIn: parent; name: "cpu"; color: Theme.accentHi; size: 18 }
                        }
                        ColumnLayout {
                            spacing: 0
                            Text {
                                text: App.reviewState === 1 ? "Reviewing…" : App.reviewState === 3 ? "Review failed" : "Review this game"
                                color: Theme.text0; font.family: Theme.sans; font.weight: Font.DemiBold; font.pixelSize: Theme.fsLg
                            }
                            Text {
                                text: App.reviewState === 1 ? "Stockfish is checking every position"
                                    : App.reviewState === 3 ? App.reviewError
                                    : "Stockfish rates every move and finds the turning points"
                                Layout.fillWidth: true
                                wrapMode: Text.WordWrap
                                color: App.reviewState === 3 ? Theme.danger : Theme.text2
                                font.family: Theme.sans; font.pixelSize: Theme.fsSm + 1
                            }
                        }
                    }
                    // progress
                    Rectangle {
                        visible: App.reviewState === 1
                        Layout.fillWidth: true; height: 6; radius: 3; color: Theme.bg4
                        Rectangle {
                            width: parent.width * App.reviewProgress; height: parent.height; radius: 3
                            color: Theme.accent
                            Behavior on width { NumberAnimation { duration: 140 } }
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Text {
                            visible: App.reviewState === 1
                            text: Math.round(App.reviewProgress * App.plyCount) + " of " + App.plyCount + " moves"
                            color: Theme.text2; font.family: Theme.mono; font.pixelSize: Theme.fsSm
                        }
                        Item { Layout.fillWidth: true }
                        AppButton {
                            visible: App.reviewState !== 1
                            variant: "primary"; icon: "zap"
                            text: App.reviewState === 3 ? "Try again" : "Run review"
                            onClicked: App.startReview()
                        }
                        AppButton {
                            visible: App.reviewState === 1
                            variant: "ghost"; compact: true; icon: "x"; text: "Cancel"
                            onClicked: App.cancelReview()
                        }
                    }
                }
            }

            // ---------- current move (coach) ----------
            Card {
                Layout.fillWidth: true
                implicitHeight: coach.implicitHeight + 32
                ColumnLayout {
                    id: coach
                    anchors.fill: parent
                    anchors.margins: 16
                    spacing: 10

                    RowLayout {
                        spacing: 10
                        ClassBadge { visible: root.cur.has; cls: root.cur.has ? root.cur.cls : -1; size: 30 }
                        ColumnLayout {
                            spacing: 0
                            Text {
                                text: root.cur.has ? root.cur.className
                                    : root.cur.ply > 0 ? root.cur.moveLabel : "Starting position"
                                color: root.cur.has ? Theme.classColor(root.cur.cls) : Theme.text0
                                font.family: Theme.sans; font.weight: Font.DemiBold; font.pixelSize: Theme.fsXl - 1
                            }
                            Text {
                                visible: root.cur.ply > 0
                                text: root.cur.moveLabel || ""
                                color: Theme.text1
                                font.family: Theme.mono; font.pixelSize: Theme.fsMd
                            }
                        }
                        Item { Layout.fillWidth: true }
                        Rectangle {   // evaluation pill
                            visible: root.cur.hasEval
                            Layout.preferredWidth: evalText.implicitWidth + 22
                            Layout.preferredHeight: 32
                            radius: 8
                            color: root.cur.winWhite >= 50 ? "#ECEFF5" : "#0B0E14"
                            border.width: 1
                            border.color: root.cur.winWhite >= 50 ? "#ECEFF5" : Theme.lineStrong
                            Text {
                                id: evalText
                                anchors.centerIn: parent
                                text: root.cur.evalText
                                color: root.cur.winWhite >= 50 ? "#10141A" : "#ECEFF5"
                                font.family: Theme.mono; font.weight: Font.Medium; font.pixelSize: Theme.fsLg
                            }
                        }
                    }

                    Text {
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        color: Theme.text1
                        font.family: Theme.sans; font.pixelSize: Theme.fsMd; lineHeight: 1.25
                        text: root.cur.has ? root.cur.comment
                            : root.cur.ply === 0 ? (root.done ? "Use ← → or the buttons below the board to walk through the game."
                                                              : "Step through the moves with ← → while the review runs, or press Run review for ratings.")
                            : "Run the review to see how this move was rated."
                    }

                    // engine's preferred move
                    Rectangle {
                        visible: root.cur.has && !root.cur.isBest && root.cur.cls !== 5 && root.cur.cls !== 10
                        Layout.fillWidth: true
                        implicitHeight: 40
                        radius: 8
                        color: Qt.rgba(0.31, 0.69, 1, 0.1)
                        border.width: 1
                        border.color: Qt.rgba(0.31, 0.69, 1, 0.3)
                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 12; anchors.rightMargin: 10
                            Icon { name: "target"; color: Theme.bestArrow; size: 16 }
                            Text {
                                text: "Best was"; color: Theme.text1
                                font.family: Theme.sans; font.pixelSize: Theme.fsMd
                            }
                            Text {
                                text: root.cur.bestBeforeSan || ""
                                color: Theme.text0
                                font.family: Theme.mono; font.weight: Font.Medium; font.pixelSize: Theme.fsMd
                            }
                            Item { Layout.fillWidth: true }
                            Text {
                                visible: (root.cur.winLoss || 0) >= 0.01
                                text: "−" + Math.round(root.cur.winLoss * 100) + "% win chance"
                                color: Theme.danger
                                font.family: Theme.mono; font.pixelSize: Theme.fsSm + 1
                            }
                            AppButton {
                                visible: (root.cur.hasLearning === true) && !App.learningMode
                                text: "Learn"; icon: "target"; variant: "primary"; compact: true
                                onClicked: App.openLearning(root.cur.ply, 0)
                            }
                        }
                    }
                }
            }

            // ---------- interactive learning line ----------
            Card {
                Layout.fillWidth: true
                visible: App.learningMode
                color: Theme.bg2
                border.width: 1.5
                border.color: Theme.accent
                implicitHeight: learningControls.implicitHeight + 28
                ColumnLayout {
                    id: learningControls
                    anchors.fill: parent
                    anchors.margins: 14
                    spacing: 9
                    RowLayout {
                        Layout.fillWidth: true
                        Text {
                            text: "MISTAKE EXPLORER"
                            color: Theme.text3; font.family: Theme.sans; font.weight: Font.DemiBold
                            font.pixelSize: Theme.fsXs; font.letterSpacing: 1.1
                        }
                        Item { Layout.fillWidth: true }
                        AppButton { text: "Return to game"; variant: "ghost"; compact: true; icon: "x"
                            onClicked: App.closeLearning() }
                    }
                    Text {
                        text: App.learningStep === 0 ? (App.learningLineName + " · starting position · 10 moves per side") : (App.learningLineName + " · ply " + App.learningStep + " of " + App.learningStepCount + " · 10 moves per side")
                        color: Theme.text0; font.family: Theme.sans; font.weight: Font.DemiBold; font.pixelSize: Theme.fsMd
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        AppButton { Layout.fillWidth: true; text: "Best continuation"; icon: "check"
                            variant: App.learningLine === 0 ? "primary" : "secondary"
                            onClicked: App.switchLearningLine(0) }
                        AppButton { Layout.fillWidth: true; text: "Opponent punishment"; icon: "target"
                            variant: App.learningLine === 1 ? "primary" : "secondary"
                            onClicked: App.switchLearningLine(1) }
                    }
                    Text {
                        Layout.fillWidth: true
                        text: "Best continuation shows what you should play. Opponent punishment starts with your actual move, then shows the strongest reply and continuation."
                        wrapMode: Text.WordWrap; color: Theme.text2; font.family: Theme.sans; font.pixelSize: Theme.fsSm
                    }
                }
            }

            // ---------- learn from mistakes ----------
            Card {
                Layout.fillWidth: true
                visible: root.done
                color: Theme.bg2
                border.width: 1.5
                border.color: App.learningMoments.length > 0 ? Theme.accent : Theme.line
                implicitHeight: learningList.implicitHeight + 28
                ColumnLayout {
                    id: learningList
                    anchors.fill: parent
                    anchors.margins: 14
                    spacing: 9
                    RowLayout {
                        Layout.fillWidth: true
                        Text {
                            text: "LEARN FROM MISTAKES"
                            color: Theme.text3; font.family: Theme.sans; font.weight: Font.DemiBold
                            font.pixelSize: Theme.fsXs; font.letterSpacing: 1.1
                        }
                        Item { Layout.fillWidth: true }
                        Text {
                            visible: App.learningMoments.length > 0
                            text: App.learningMoments.length + (App.learningMoments.length === 1 ? " moment" : " moments")
                            color: Theme.accent; font.family: Theme.mono; font.weight: Font.DemiBold; font.pixelSize: Theme.fsSm
                        }
                    }
                    Text {
                        visible: App.learningMoments.length > 0
                        text: "Compare the engine's best continuation with the opponent's strongest punishment line."
                        Layout.fillWidth: true; wrapMode: Text.WordWrap
                        color: Theme.text2; font.family: Theme.sans; font.pixelSize: Theme.fsSm
                    }
                    AppButton {
                        visible: App.learningMoments.length > 0 && !App.learningMode
                        Layout.fillWidth: true
                        variant: "primary"
                        icon: "target"
                        text: "Practice Mistakes (" + App.learningMoments.length + ")"
                        onClicked: App.openLearning(App.learningMoments[0].ply, 0)
                    }
                    RowLayout {
                        visible: App.learningMoments.length === 0
                        Layout.fillWidth: true
                        spacing: 8
                        Icon { name: "check"; color: Theme.success; size: 18 }
                        Text {
                            text: "Clean game! No major mistakes detected to review."
                            color: Theme.text1; font.family: Theme.sans; font.pixelSize: Theme.fsSm
                            Layout.fillWidth: true
                        }
                    }
                    Repeater {
                        model: App.learningMoments
                        delegate: Rectangle {
                            required property var modelData
                            Layout.fillWidth: true
                            implicitHeight: 88
                            radius: 10
                            color: Theme.bg2
                            border.width: 1
                            border.color: Theme.line
                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 10; anchors.rightMargin: 8
                                spacing: 8
                                ClassBadge { cls: modelData.cls; size: 22 }
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 2
                                    Text { text: modelData.label; color: Theme.text0; font.family: Theme.mono; font.pixelSize: Theme.fsSm + 1 }
                                    Text { text: modelData.detail; color: Theme.danger; font.family: Theme.sans; font.pixelSize: Theme.fsXs + 1 }
                                    Text { text: "Best: " + (modelData.bestMove || "—"); color: Theme.bestArrow; font.family: Theme.sans; font.weight: Font.DemiBold; font.pixelSize: Theme.fsXs + 1 }
                                }
                                ColumnLayout {
                                    spacing: 5
                                    AppButton { text: "Best line"; icon: "check"; variant: "primary"; compact: true
                                        onClicked: App.openLearning(modelData.ply, 0) }
                                    AppButton { text: "Punishment"; icon: "target"; variant: "secondary"; compact: true
                                        onClicked: App.openLearning(modelData.ply, 1) }
                                }
                            }
                        }
                    }
                }
            }


            // ---------- accuracy & rating ----------
            Card {
                Layout.fillWidth: true
                visible: root.done
                implicitHeight: (root.sum.whiteRating > 0 || root.sum.blackRating > 0) ? 198 : 176
                Row {
                    anchors.centerIn: parent
                    spacing: 44
                    AccuracyRing {
                        value: root.sum.whiteAccuracy || 0
                        rating: root.sum.whiteRating || 0
                        label: App.gameInfo.white || "White"
                        sublabel: "White accuracy"
                    }
                    Rectangle { width: 1; height: (root.sum.whiteRating > 0 || root.sum.blackRating > 0) ? 110 : 100; color: Theme.line; anchors.verticalCenter: parent.verticalCenter; anchors.verticalCenterOffset: (root.sum.whiteRating > 0 || root.sum.blackRating > 0) ? -12 : -18 }
                    AccuracyRing {
                        value: root.sum.blackAccuracy || 0
                        rating: root.sum.blackRating || 0
                        label: App.gameInfo.black || "Black"
                        sublabel: "Black accuracy"
                    }
                }
            }

            // ---------- phase accuracy ----------
            Card {
                Layout.fillWidth: true
                visible: root.done && ((root.sum.whiteOpeningAccuracy || 0) > 0 || (root.sum.blackOpeningAccuracy || 0) > 0)
                implicitHeight: phaseCol.implicitHeight + 28

                ColumnLayout {
                    id: phaseCol
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: 14
                    spacing: 8

                    Text {
                        text: "PHASE ACCURACY"
                        color: Theme.text3
                        font.family: Theme.sans
                        font.weight: Font.DemiBold
                        font.pixelSize: Theme.fsXs
                        font.letterSpacing: 1.1
                    }

                    // Opening
                    RowLayout {
                        Layout.fillWidth: true
                        Text {
                            text: ((root.sum.whiteOpeningAccuracy || 0) > 0 ? root.sum.whiteOpeningAccuracy.toFixed(1) + "%" : "-")
                            color: Theme.text0; font.family: Theme.mono; font.weight: Font.Medium; font.pixelSize: Theme.fsSm
                            Layout.preferredWidth: 60
                        }
                        Text {
                            text: "Opening"
                            color: Theme.text2; font.family: Theme.sans; font.pixelSize: Theme.fsSm
                            Layout.fillWidth: true; horizontalAlignment: Text.AlignHCenter
                        }
                        Text {
                            text: ((root.sum.blackOpeningAccuracy || 0) > 0 ? root.sum.blackOpeningAccuracy.toFixed(1) + "%" : "-")
                            color: Theme.text0; font.family: Theme.mono; font.weight: Font.Medium; font.pixelSize: Theme.fsSm
                            Layout.preferredWidth: 60; horizontalAlignment: Text.AlignRight
                        }
                    }

                    // Middlegame
                    RowLayout {
                        Layout.fillWidth: true
                        visible: (root.sum.whiteMiddlegameAccuracy || 0) > 0 || (root.sum.blackMiddlegameAccuracy || 0) > 0
                        Text {
                            text: ((root.sum.whiteMiddlegameAccuracy || 0) > 0 ? root.sum.whiteMiddlegameAccuracy.toFixed(1) + "%" : "-")
                            color: Theme.text0; font.family: Theme.mono; font.weight: Font.Medium; font.pixelSize: Theme.fsSm
                            Layout.preferredWidth: 60
                        }
                        Text {
                            text: "Middlegame"
                            color: Theme.text2; font.family: Theme.sans; font.pixelSize: Theme.fsSm
                            Layout.fillWidth: true; horizontalAlignment: Text.AlignHCenter
                        }
                        Text {
                            text: ((root.sum.blackMiddlegameAccuracy || 0) > 0 ? root.sum.blackMiddlegameAccuracy.toFixed(1) + "%" : "-")
                            color: Theme.text0; font.family: Theme.mono; font.weight: Font.Medium; font.pixelSize: Theme.fsSm
                            Layout.preferredWidth: 60; horizontalAlignment: Text.AlignRight
                        }
                    }

                    // Endgame
                    RowLayout {
                        Layout.fillWidth: true
                        visible: (root.sum.whiteEndgameAccuracy || 0) > 0 || (root.sum.blackEndgameAccuracy || 0) > 0
                        Text {
                            text: ((root.sum.whiteEndgameAccuracy || 0) > 0 ? root.sum.whiteEndgameAccuracy.toFixed(1) + "%" : "-")
                            color: Theme.text0; font.family: Theme.mono; font.weight: Font.Medium; font.pixelSize: Theme.fsSm
                            Layout.preferredWidth: 60
                        }
                        Text {
                            text: "Endgame"
                            color: Theme.text2; font.family: Theme.sans; font.pixelSize: Theme.fsSm
                            Layout.fillWidth: true; horizontalAlignment: Text.AlignHCenter
                        }
                        Text {
                            text: ((root.sum.blackEndgameAccuracy || 0) > 0 ? root.sum.blackEndgameAccuracy.toFixed(1) + "%" : "-")
                            color: Theme.text0; font.family: Theme.mono; font.weight: Font.Medium; font.pixelSize: Theme.fsSm
                            Layout.preferredWidth: 60; horizontalAlignment: Text.AlignRight
                        }
                    }
                }
            }

            // ---------- key moments ----------
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 8
                visible: root.done && App.keyMoments.length > 0
                Text {
                    text: "KEY MOMENTS"
                    color: Theme.text3; font.family: Theme.sans; font.weight: Font.DemiBold; font.pixelSize: Theme.fsXs
                    font.letterSpacing: 1.1
                }
                Flow {
                    Layout.fillWidth: true
                    spacing: 8
                    Repeater {
                        model: App.keyMoments
                        delegate: Rectangle {
                            required property var modelData
                            readonly property bool active: App.currentPly === modelData.ply
                            height: 32
                            width: momentRow.implicitWidth + 20
                            radius: 16
                            color: active ? Theme.bg4 : tap.pressed ? Theme.bg4 : hover.hovered ? Theme.bg3 : Theme.bg2
                            border.width: 1
                            border.color: active ? Theme.classColor(modelData.cls) : Theme.line
                            Behavior on color { ColorAnimation { duration: 90 } }
                            Row {
                                id: momentRow
                                anchors.centerIn: parent
                                spacing: 7
                                ClassBadge { cls: modelData.cls; size: 18; anchors.verticalCenter: parent.verticalCenter }
                                Text {
                                    anchors.verticalCenter: parent.verticalCenter
                                    text: modelData.label
                                    color: Theme.text0
                                    font.family: Theme.mono; font.pixelSize: Theme.fsSm + 1
                                }
                                Text {
                                    anchors.verticalCenter: parent.verticalCenter
                                    text: modelData.detail
                                    color: Theme.text2
                                    font.family: Theme.sans; font.pixelSize: Theme.fsXs + 1
                                }
                            }
                            HoverHandler { id: hover; cursorShape: Qt.PointingHandCursor }
                            TapHandler { id: tap; onTapped: App.goTo(modelData.ply) }
                        }
                    }
                }
            }

            // ---------- breakdown ----------
            Card {
                Layout.fillWidth: true
                visible: root.done
                implicitHeight: breakdown.implicitHeight + 28
                ColumnLayout {
                    id: breakdown
                    anchors.fill: parent
                    anchors.margins: 14
                    spacing: 2

                    RowLayout {
                        Layout.fillWidth: true
                        Layout.bottomMargin: 6
                        Text {
                            text: "MOVE QUALITY"
                            color: Theme.text3; font.family: Theme.sans; font.weight: Font.DemiBold; font.pixelSize: Theme.fsXs
                            font.letterSpacing: 1.1
                        }
                        Item { Layout.fillWidth: true }
                        Text { text: "White"; Layout.preferredWidth: 46; horizontalAlignment: Text.AlignRight
                            color: Theme.text3; font.family: Theme.sans; font.pixelSize: Theme.fsXs + 1 }
                        Text { text: "Black"; Layout.preferredWidth: 46; horizontalAlignment: Text.AlignRight
                            color: Theme.text3; font.family: Theme.sans; font.pixelSize: Theme.fsXs + 1 }
                    }
                    Repeater {
                        model: 11
                        delegate: Rectangle {
                            required property int index
                            readonly property int w: (root.sum.whiteCounts || [])[index] || 0
                            readonly property int b: (root.sum.blackCounts || [])[index] || 0
                            Layout.fillWidth: true
                            implicitHeight: (w + b) > 0 ? 30 : 0
                            visible: (w + b) > 0
                            radius: 6
                            color: rowTap.pressed ? Theme.bg4 : rowHover.hovered ? Theme.bg3 : "transparent"
                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 6; anchors.rightMargin: 4
                                spacing: 10
                                ClassBadge { cls: index; size: 18 }
                                Text {
                                    text: Theme.className(index)
                                    color: Theme.text0
                                    font.family: Theme.sans; font.pixelSize: Theme.fsMd
                                }
                                Item { Layout.fillWidth: true }
                                Text { text: w; Layout.preferredWidth: 46; horizontalAlignment: Text.AlignRight
                                    color: w > 0 ? Theme.text0 : Theme.text3; font.family: Theme.mono; font.pixelSize: Theme.fsMd }
                                Text { text: b; Layout.preferredWidth: 46; horizontalAlignment: Text.AlignRight
                                    color: b > 0 ? Theme.text0 : Theme.text3; font.family: Theme.mono; font.pixelSize: Theme.fsMd }
                            }
                            HoverHandler { id: rowHover; cursorShape: Qt.PointingHandCursor }
                            TapHandler { id: rowTap; onTapped: App.jumpToClass(index, 1) }
                        }
                    }
                }
            }
        }
    }
}
