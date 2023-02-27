/*
    SPDX-FileCopyrightText: 2022 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "layers.h"
#include "rules/types.h"
#include "scene.h"
#include "x11/net/net.h"

namespace KWin::win
{

template<typename Win>
void set_keep_below(Win* win, bool keep);

template<typename Win>
void set_keep_above(Win* win, bool keep)
{
    keep = win->control->rules.checkKeepAbove(keep);
    if (keep && !win->control->rules.checkKeepBelow(false)) {
        set_keep_below(win, false);
    }
    if (keep == win->control->keep_above) {
        // force hint change if different
        if constexpr (requires(Win win) { win.net_info; }) {
            if (static_cast<bool>(win->net_info->state() & x11::net::KeepAbove) != keep) {
                win->net_info->setState(keep ? x11::net::KeepAbove : x11::net::States(),
                                        x11::net::KeepAbove);
            }
        }
        return;
    }
    win->control->keep_above = keep;

    if constexpr (requires(Win win) { win.net_info; }) {
        win->net_info->setState(keep ? x11::net::KeepAbove : x11::net::States(),
                                x11::net::KeepAbove);
    }

    update_layer(win);
    win->updateWindowRules(rules::type::above);

    if constexpr (requires(Win * win) { win->doSetKeepAbove(); }) {
        win->doSetKeepAbove();
    }

    Q_EMIT win->qobject->keepAboveChanged(keep);
}

template<typename Win>
void set_keep_below(Win* win, bool keep)
{
    keep = win->control->rules.checkKeepBelow(keep);
    if (keep && !win->control->rules.checkKeepAbove(false)) {
        set_keep_above(win, false);
    }
    if (keep == win->control->keep_below) {
        // force hint change if different
        if constexpr (requires(Win win) { win.net_info; }) {
            if (static_cast<bool>(win->net_info->state() & x11::net::KeepBelow) != keep) {
                win->net_info->setState(keep ? x11::net::KeepBelow : x11::net::States(),
                                        x11::net::KeepBelow);
            }
        }

        return;
    }
    win->control->keep_below = keep;

    if constexpr (requires(Win win) { win.net_info; }) {
        win->net_info->setState(keep ? x11::net::KeepBelow : x11::net::States(),
                                x11::net::KeepBelow);
    }

    update_layer(win);
    win->updateWindowRules(rules::type::below);

    if constexpr (requires(Win * win) { win->doSetKeepBelow(); }) {
        win->doSetKeepBelow();
    }

    Q_EMIT win->qobject->keepBelowChanged(keep);
}

template<typename Win>
void set_minimized(Win* win, bool set, bool avoid_animation = false)
{
    if (set) {
        if (!win->isMinimizable() || win->control->minimized)
            return;

        win->control->minimized = true;
        if constexpr (requires(Win win) { win.doMinimize(); }) {
            win->doMinimize();
        }

        win->updateWindowRules(rules::type::minimize);
        win->space.base.render->compositor->addRepaint(visible_rect(win));

        // TODO: merge signal with s_minimized
        Q_EMIT win->qobject->clientMinimized(!avoid_animation);
        Q_EMIT win->qobject->minimizedChanged();
    } else {
        if (!win->control->minimized) {
            return;
        }
        if (win->control->rules.checkMinimize(false)) {
            return;
        }

        win->control->minimized = false;
        if constexpr (requires(Win win) { win.doMinimize(); }) {
            win->doMinimize();
        }

        win->updateWindowRules(rules::type::minimize);
        Q_EMIT win->qobject->clientUnminimized(!avoid_animation);
        Q_EMIT win->qobject->minimizedChanged();
    }
}

template<typename Win>
void propagate_minimized_to_transients(Win& window)
{
    if (window.control->minimized) {
        for (auto win : window.transient->children) {
            if (!win->control) {
                continue;
            }
            if (win->transient->modal()) {
                // There's no reason to hide modal dialogs with the main client...
                continue;
            }
            // ... but to keep them to eg. watch progress or whatever.
            if (!win->control->minimized) {
                set_minimized(win, true);
                propagate_minimized_to_transients(*win);
            }
        }
        if (window.transient->modal()) {
            // If a modal dialog is minimized, minimize its mainwindow too.
            for (auto c2 : window.transient->leads()) {
                set_minimized(c2, true);
            }
        }
    } else {
        for (auto win : window.transient->children) {
            if (!win->control) {
                continue;
            }
            if (win->control->minimized) {
                set_minimized(win, false);
                propagate_minimized_to_transients(*win);
            }
        }
        if (window.transient->modal()) {
            for (auto c2 : window.transient->leads()) {
                set_minimized(c2, false);
            }
        }
    }
}

}
