// SPDX-FileCopyrightText: 2023 Roman Gilg <subdiff@gmail.com>
//
// SPDX-License-Identifier: GPL-2.0-or-later

effects.desktopChanged.connect(function(old, current) {
    var stackingOrder = effects.stackingOrder;
    for (var i=0; i<stackingOrder.length; i++) {
        var w = stackingOrder[i];
        //1 second long random animation, marked as fullscreen
        w.anim1 = animate({
            window: w,
            duration: 1000,
            animations: [{
                type: Effect.Scale,
                curve: Effect.GaussianCurve,
                to: 1.4,
                fullScreen: true
            }, {
                type: Effect.Opacity,
                curve: Effect.GaussianCurve,
                to: 0.0,
                fullScreen: true
            }]
        });
    }
});
