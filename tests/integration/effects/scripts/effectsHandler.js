// SPDX-FileCopyrightText: 2023 Roman Gilg <subdiff@gmail.com>
//
// SPDX-License-Identifier: GPL-2.0-or-later

effects.windowAdded.connect(function(window) {
    sendTestResponse("windowAdded - " + window.caption);
    sendTestResponse("stackingOrder - " + effects.stackingOrder.length + " " + effects.stackingOrder[0].caption);

    window.minimizedChanged.connect(() => {
        if (window.minimized) {
            sendTestResponse("windowMinimized - " + window.caption);
        } else {
            sendTestResponse("windowUnminimized - " + window.caption);
        }
    });
});
effects.windowClosed.connect(function(window) {
    sendTestResponse("windowClosed - " + window.caption);
});
effects.desktopChanged.connect(function(old, current) {
    sendTestResponse("desktopChanged - " + old.x11DesktopNumber + " " + current.x11DesktopNumber);
});
