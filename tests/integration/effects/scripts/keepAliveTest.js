// SPDX-FileCopyrightText: 2023 Roman Gilg <subdiff@gmail.com>
//
// SPDX-License-Identifier: GPL-2.0-or-later

"use strict";

effects.windowClosed.connect(function (window) {
    animate({
        window: window,
        type: Effect.Scale,
        duration: 1000,
        from: 1.0,
        to: 0.0
        // by default, keepAlive is set to true
    });
    sendTestResponse("triggered");
});
