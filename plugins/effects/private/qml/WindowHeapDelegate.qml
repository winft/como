/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>
    SPDX-FileCopyrightText: 2022 ivan tkachenko <me@ratijas.tk>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

import QtQuick
import QtQuick.Window
import Qt5Compat.GraphicalEffects
import org.kde.kirigami as Kirigami
import org.kde.kwin as KWinComponents
import org.kde.kwin.private.effects
import org.kde.plasma.components 3.0 as PC3
import org.kde.plasma.extras as PlasmaExtras
import org.kde.ksvg 1.0 as KSvg

Item {
    id: thumb

    required property QtObject window
    required property int index
    required property Item windowHeap

    readonly property bool selected: windowHeap.selectedIndex === index
    property real partialActivationFactor: effect.partialActivationFactor
    property bool gestureInProgress: effect.gestureInProgress

    // no desktops is a special value which means "All Desktops"
    readonly property bool presentOnCurrentDesktop: !window.desktops.length || window.desktops.indexOf(KWinComponents.Workspace.currentDesktop) !== -1
    readonly property bool initialHidden: window.minimized || !presentOnCurrentDesktop
    readonly property bool activeHidden: {
        if (window.skipSwitcher) {
            return true;
        } else if (windowHeap.showOnly === "activeClass") {
            if (!KWinComponents.Workspace.activeWindow) {
                return true;
            } else {
                return KWinComponents.Workspace.activeWindow.resourceName !== window.resourceName;
            }
        } else {
            return windowHeap.showOnly.length !== 0
                && windowHeap.showOnly.indexOf(window.internalId) === -1;
        }
    }

    // Show a close button on this thumbnail
    property bool closeButtonVisible: true
    // Show a text label under this thumbnail
    property bool windowTitleVisible: true

    // Same as for window heap
    property bool animationEnabled: false

    //scale up and down the whole thumbnail without affecting layouting
    property real targetScale: 1.0

    property DragManager activeDragHandler: dragHandler

    // Swipe down gesture by touch, in some effects will close the window
    readonly property alias downGestureProgress: touchDragHandler.downGestureProgress
    signal downGestureTriggered()

    // "normal" | "pressed" | "drag" | "reparenting"
    property string substate: "normal"

    state: {
        if (thumb.gestureInProgress) {
            return "partial";
        }
        if (thumb.partialActivationFactor > 0.5 && (cell.isReady || activeHidden)) {
            return activeHidden ? "active-hidden" : `active-${substate}`;
        }
        return initialHidden ? "initial-hidden" : "initial";
    }

    visible: opacity > 0
    z: (activeDragHandler.active || returning.running) ? 1000
        : window.stackingOrder * (presentOnCurrentDesktop ? 1 : 0.001)

    function restoreDND(oldGlobalRect: rect) {
        thumbSource.restoreDND(oldGlobalRect);
    }

    component TweenBehavior : Behavior {
        enabled: thumb.state === "active-normal" && thumb.windowHeap.animationEnabled && thumb.animationEnabled && !thumb.activeDragHandler.active
        NumberAnimation {
            duration: thumb.windowHeap.animationDuration
            easing.type: Easing.InOutCubic
        }
    }

    TweenBehavior on x {}
    TweenBehavior on y {}
    TweenBehavior on width {}
    TweenBehavior on height {}

    KWinComponents.WindowThumbnail {
        id: thumbSource
        wId: thumb.window.internalId

        Drag.proposedAction: Qt.MoveAction
        Drag.supportedActions: Qt.MoveAction
        Drag.source: thumb.window
        Drag.hotSpot: Qt.point(
            thumb.activeDragHandler.centroid.pressPosition.x * thumb.targetScale,
            thumb.activeDragHandler.centroid.pressPosition.y * thumb.targetScale)
        Drag.keys: ["kwin-window"]

        onXChanged: effect.checkItemDraggedOutOfScreen(thumbSource)
        onYChanged: effect.checkItemDraggedOutOfScreen(thumbSource)

        function saveDND() {
            const oldGlobalRect = mapToItem(null, 0, 0, width, height);
            thumb.windowHeap.saveDND(thumb.window.internalId, oldGlobalRect);
        }
        function restoreDND(oldGlobalRect: rect) {
            thumb.substate = "reparenting";

            const newGlobalRect = mapFromItem(null, oldGlobalRect);

            x = newGlobalRect.x;
            y = newGlobalRect.y;
            width = newGlobalRect.width;
            height = newGlobalRect.height;

            thumb.substate = "normal";
        }
        function deleteDND() {
            thumb.windowHeap.deleteDND(thumb.window.internalId);
        }

        // Not using FrameSvg hover element intentionally for stylistic reasons
        Rectangle {
            border.width: Kirigami.Units.largeSpacing
            border.color: Kirigami.Theme.highlightColor
            anchors.fill: parent
            anchors.margins: -border.width
            radius: Kirigami.Units.cornerRadius
            color: "transparent"
            visible: !thumb.windowHeap.dragActive && (hoverHandler.hovered || (thumb.selected && Window.window.activeFocusItem)) && windowHeap.effectiveOrganized
        }

        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.NoButton
            cursorShape: thumb.activeDragHandler.active ? Qt.ClosedHandCursor : Qt.ArrowCursor
        }
    }

    PC3.Label {
        anchors.fill: thumbSource
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        text: i18nd("kwin", "Drag Down To Close")
        opacity: thumbSource.opacity
        visible: !thumb.activeHidden && touchDragHandler.active
    }

    Kirigami.Icon {
        id: icon
        width: Kirigami.Units.iconSizes.large
        height: Kirigami.Units.iconSizes.large
        source: thumb.window.icon
        anchors.horizontalCenter: thumbSource.horizontalCenter
        anchors.bottom: thumbSource.bottom
        anchors.bottomMargin: -Math.round(height / 4)
        visible: !thumb.activeHidden && !activeDragHandler.active


        PlasmaExtras.ShadowedLabel {
            id: caption
            visible: thumb.windowTitleVisible
            width: cell.width
            maximumLineCount: 1
            anchors.top: parent.bottom
            anchors.horizontalCenter: parent.horizontalCenter
            elide: Text.ElideRight
            text: thumb.window.caption
            textFormat: Text.PlainText
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
    }

    ExpoCell {
        id: cell
        layout: windowHeap.layout
        enabled: !thumb.activeHidden
        naturalX: thumb.window.x
        naturalY: thumb.window.y
        naturalWidth: thumb.window.width
        naturalHeight: thumb.window.height
        persistentKey: thumb.window.internalId
        bottomMargin: icon.height / 4 + (thumb.windowTitleVisible ? caption.height : 0)
        property bool isReady: width !== 0 && height !== 0
    }

    states: [
        State {
            name: "initial"
            PropertyChanges {
                target: thumb
                x: thumb.window.x - targetScreen.geometry.x - (thumb.windowHeap.absolutePositioning ?  windowHeap.layout.Kirigami.ScenePosition.x : 0)
                y: thumb.window.y - targetScreen.geometry.y - (thumb.windowHeap.absolutePositioning ?  windowHeap.layout.Kirigami.ScenePosition.y : 0)
                width: thumb.window.width
                height: thumb.window.height
            }
            PropertyChanges {
                target: thumbSource
                x: 0
                y: 0
                width: thumb.window.width
                height: thumb.window.height
            }
            PropertyChanges {
                target: icon
                opacity: 0
            }
            PropertyChanges {
                target: closeButton
                opacity: 0
            }
        },
        State {
            name: "partial"
            PropertyChanges {
                target: thumb
                x: (thumb.window.x - targetScreen.geometry.x - (thumb.windowHeap.absolutePositioning ?  windowHeap.layout.Kirigami.ScenePosition.x : 0)) * (1 - thumb.partialActivationFactor) + cell.x * thumb.partialActivationFactor
                y: (thumb.window.y - targetScreen.geometry.y - (thumb.windowHeap.absolutePositioning ?  windowHeap.layout.Kirigami.ScenePosition.y : 0)) * (1 - thumb.partialActivationFactor) + cell.y * thumb.partialActivationFactor
                width: thumb.window.width * (1 - thumb.partialActivationFactor) + cell.width * thumb.partialActivationFactor
                height: thumb.window.height * (1 - thumb.partialActivationFactor) + cell.height * thumb.partialActivationFactor
                opacity: thumb.initialHidden
                    ? (thumb.activeHidden ? 0 : thumb.partialActivationFactor)
                    : (thumb.activeHidden ? 1 - thumb.partialActivationFactor : 1)
            }
            PropertyChanges {
                target: thumbSource
                x: 0
                y: 0
                width: thumb.width
                height: thumb.height
            }
            PropertyChanges {
                target: icon
                opacity: thumb.partialActivationFactor
            }
            PropertyChanges {
                target: closeButton
                opacity: thumb.partialActivationFactor
            }
        },
        State {
            name: "initial-hidden"
            extend: "initial"
            PropertyChanges {
                target: thumb
                opacity: 0
            }
            PropertyChanges {
                target: icon
                opacity: 0
            }
            PropertyChanges {
                target: closeButton
                opacity: 0
            }
        },
        State {
            name: "active-hidden"
            extend: "initial-hidden"
        },
        State {
            // this state is never directly used without a substate
            name: "active"
            PropertyChanges {
                target: thumb
                x: cell.x
                y: cell.y
                width: cell.width
                height: cell.height
            }
            PropertyChanges {
                target: icon
                opacity: 1
            }
            PropertyChanges {
                target: closeButton
                opacity: 1
            }
        },
        State {
            name: "active-normal"
            extend: "active"
            PropertyChanges {
                target: thumbSource
                x: 0
                y: 0
                width: thumb.width
                height: thumb.height
            }
        },
        State {
            name: "active-pressed"
            extend: "active"
            PropertyChanges {
                target: thumbSource
                width: thumb.width
                height: thumb.height
            }
        },
        State {
            name: "active-drag"
            extend: "active"
            PropertyChanges {
                target: thumbSource
                x: -thumb.activeDragHandler.centroid.pressPosition.x * thumb.targetScale +
                        thumb.activeDragHandler.centroid.position.x
                y: -thumb.activeDragHandler.centroid.pressPosition.y * thumb.targetScale +
                        thumb.activeDragHandler.centroid.position.y
                width: thumb.width * thumb.targetScale
                height: thumb.height * thumb.targetScale
            }
        },
        State {
            name: "active-reparenting"
            extend: "active"
        }
    ]

    transitions: [
        Transition {
            id: returning
            from: "active-drag, active-reparenting"
            to: "active-normal"
            enabled: thumb.windowHeap.animationEnabled
            NumberAnimation {
                duration: thumb.windowHeap.animationDuration
                properties: "x, y, width, height"
                easing.type: Easing.InOutCubic
            }
        }
    ]

    HoverHandler {
        id: hoverHandler
        onHoveredChanged: if (hovered !== selected) {
            thumb.windowHeap.resetSelected();
        }
    }

    TapHandler {
        acceptedButtons: Qt.LeftButton
        onTapped: {
            KWinComponents.Workspace.activeWindow = thumb.window;
            thumb.windowHeap.activated();
        }
        onPressedChanged: {
            if (pressed) {
                var saved = Qt.point(thumbSource.x, thumbSource.y);
                thumbSource.Drag.active = true;
                thumb.substate = "pressed";
                thumbSource.x = saved.x;
                thumbSource.y = saved.y;
            } else if (!thumb.activeDragHandler.active) {
                thumbSource.Drag.active = false;
                thumb.substate = "normal";
            }
        }
    }

    component DragManager : DragHandler {
        target: null
        grabPermissions: PointerHandler.CanTakeOverFromAnything
        // This does not work when moving pointer fast and pressing along the way
        // See also QTBUG-105903, QTBUG-105904
        // enabled: thumb.state !== "active-normal"

        onActiveChanged: {
            thumb.windowHeap.dragActive = active;
            if (active) {
                thumb.activeDragHandler = this;
                thumb.substate = "drag";
            } else {
                thumbSource.saveDND();

                var action = thumbSource.Drag.drop();
                if (action === Qt.MoveAction) {
                    // This whole component is in the process of being destroyed due to drop onto
                    // another virtual desktop (not another screen).
                    if (typeof thumbSource !== "undefined") {
                        // Except the case when it was dropped on the same desktop which it's already on, so let's return to normal state anyway.
                        thumbSource.deleteDND();
                        thumb.substate = "normal";
                    }
                    return;
                }

                var globalPos = targetScreen.mapToGlobal(centroid.scenePosition);
                effect.checkItemDroppedOutOfScreen(globalPos, thumbSource);

                if (typeof thumbSource !== "undefined") {
                    // else, return to normal without reparenting
                    thumbSource.deleteDND();
                    thumb.substate = "normal";
                }
            }
        }
    }

    DragManager {
        id: dragHandler
        acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad | PointerDevice.Stylus
    }

    DragManager {
        id: touchDragHandler
        acceptedDevices: PointerDevice.TouchScreen
        readonly property double downGestureProgress: {
            if (!active) {
                return 0.0;
            }

            const startDistance = thumb.windowHeap.Kirigami.ScenePosition.y + thumb.windowHeap.height - centroid.scenePressPosition.y;
            const localPosition = thumb.windowHeap.Kirigami.ScenePosition.y + thumb.windowHeap.height - centroid.scenePosition.y;
            return 1 - Math.min(localPosition/startDistance, 1);
        }

        onActiveChanged: {
            if (!active) {
                if (downGestureProgress > 0.6) {
                    thumb.downGestureTriggered();
                }
            }
        }
    }

    Loader {
        id: closeButton
        LayoutMirroring.enabled: Qt.application.layoutDirection === Qt.RightToLeft

        anchors {
            right: thumbSource.right
            top: thumbSource.top
            margins: Kirigami.Units.smallSpacing
        }
        active: thumb.closeButtonVisible && (hoverHandler.hovered || Kirigami.Settings.tabletMode || Kirigami.Settings.hasTransientTouchInput) && thumb.window.closeable && !thumb.activeDragHandler.active

        sourceComponent: PC3.Button {
            text: i18ndc("kwin", "@info:tooltip as in: 'close this window'", "Close window")
            icon.name: "window-close"
            display: PC3.AbstractButton.IconOnly

            PC3.ToolTip.text: text
            PC3.ToolTip.visible: hovered && display === PC3.AbstractButton.IconOnly
            PC3.ToolTip.delay: Kirigami.Units.toolTipDelay
            Accessible.name: text

            onClicked: thumb.window.closeWindow();
        }
    }

    Component.onDestruction: {
        if (selected) {
            windowHeap.resetSelected();
        }
    }
}
