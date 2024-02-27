// SPDX-FileCopyrightText: 2023 Roman Gilg <subdiff@gmail.com>
//
// SPDX-License-Identifier: GPL-2.0-or-later

effects.windowAdded.connect(function(w) {
    w.anim1 = animate({
        window: w,
        duration: 100,
        animations: [{
            type: Effect.Scale,
            to: 1.4,
            curve: QEasingCurve.OutCubic
        }, {
            type: Effect.Opacity,
            curve: QEasingCurve.OutCubic,
            to: 0.0
        }]
    });
    sendTestResponse(typeof(w.anim1) == "object" && Array.isArray(w.anim1));

    w.minimizedChanged.connect(() => {
        if (w.minimized) {
            retarget(w.anim1, 1.5, 200);
        } else {
            cancel(w.anim1);
        }
    });
});
