/*
    SPDX-FileCopyrightText: 2022 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <como/base/options.h>
#include <como/utils/algorithm.h>
#include <como/win/desktop_get.h>
#include <como/win/layers.h>
#include <como/win/util.h>

#include <xcb/xcb.h>

namespace como::win::x11
{

// focus_in -> the window got FocusIn event
// ignore_subspace - call comes from _NET_ACTIVE_WINDOW message, don't refuse just because of window
//     is on a different subspace
template<typename Space, typename Win>
bool allow_window_activation(Space& space,
                             Win const* window,
                             xcb_timestamp_t time = -1U,
                             bool focus_in = false,
                             bool ignore_subspace = false)
{
    using var_win = typename Space::window_t;

    // space.options->focusStealingPreventionLevel :
    // 0 - none    - old KWin behaviour, new windows always get focus
    // 1 - low     - focus stealing prevention is applied normally, when unsure, activation is
    // allowed 2 - normal  - focus stealing prevention is applied normally, when unsure, activation
    // is not allowed,
    //              this is the default
    // 3 - high    - new window gets focus only if it belongs to the active application,
    //              or when no window is currently active
    // 4 - extreme - no window gets focus without user intervention
    if (time == -1U) {
        time = window->userTime();
    }

    auto level
        = window->control->rules.checkFSP(space.options->qobject->focusStealingPreventionLevel());
    if (space.session_manager->state() == session_state::saving
        && enum_index(level) <= enum_index(fsp_level::medium)) {
        // <= normal
        return true;
    }

    auto ac = most_recently_activated_window(space);
    if (focus_in) {
        auto& sgf = space.stacking.should_get_focus;
        if (std::find(sgf.cbegin(), sgf.cend(), var_win(const_cast<Win*>(window))) != sgf.cend()) {
            // FocusIn was result of KWin's action
            return true;
        }
        // Before getting FocusIn, the active Client already
        // got FocusOut, and therefore got deactivated.
        ac = space.stacking.last_active;
    }
    if (time == 0) {
        // explicitly asked not to get focus
        if (!window->control->rules.checkAcceptFocus(false))
            return false;
    }

    auto const protection = ac
        ? std::visit(
              overload{[](auto&& win) { return win->control->rules.checkFPP(fsp_level::medium); }},
              *ac)
        : fsp_level::none;

    // stealing is unconditionally allowed (NETWM behavior)
    if (level == fsp_level::none || protection == fsp_level::none) {
        return true;
    }

    // The active client "grabs" the focus or stealing is generally forbidden
    if (level == fsp_level::extreme || protection == fsp_level::extreme) {
        return false;
    }

    // subspace switching is only allowed in the "no protection" case
    if (!ignore_subspace && !on_current_subspace(*window)) {
        // allow only with level == 0
        return false;
    }

    // No active client, it's ok to pass focus
    // NOTICE that extreme protection needs to be handled before to allow protection on unmanged
    // windows
    if (!ac || std::visit(overload{[](auto&& win) { return is_desktop(win); }}, *ac)) {
        qCDebug(KWIN_CORE) << "Activation: No client active, allowing";
        // no active client -> always allow
        return true;
    }

    // TODO window urgency  -> return true?

    // Unconditionally allow intra-client passing around for lower stealing protections
    // unless the active client has High interest
    if (std::visit(overload{[&](auto&& win) {
                       return belong_to_same_client(
                           window, win, same_client_check::relaxed_for_active);
                   }},
                   *ac)
        && protection < fsp_level::high) {
        qCDebug(KWIN_CORE) << "Activation: Belongs to active application";
        return true;
    }

    if (!on_current_subspace(*window)) {
        // we allowed explicit self-activation across subspaces inside a client or if no client was
        // active, but not otherwise
        return false;
    }

    // High FPS, not intr-client change. Only allow if the active client has only minor interest
    if (level > fsp_level::medium && protection > fsp_level::low) {
        return false;
    }

    if (time == -1U) { // no time known
        qCDebug(KWIN_CORE) << "Activation: No timestamp at all";
        // Only allow for Low protection unless active client has High interest in focus
        if (level < fsp_level::medium && protection < fsp_level::high) {
            return true;
        }

        // no timestamp at all, don't activate - because there's also creation timestamp
        // done on CreateNotify, this case should happen only in case application
        // maps again already used window, i.e. this won't happen after app startup
        return false;
    }

    // Low or medium FSP level, usertime comparism is possible
    auto const user_time = std::visit(overload{[](auto&& win) { return win->userTime(); }}, *ac);
    qCDebug(KWIN_CORE) << "Activation, compared:" << window << ":" << time << ":" << user_time
                       << ":" << (net::timestampCompare(time, user_time) >= 0);

    // time >= user_time
    return net::timestampCompare(time, user_time) >= 0;
}

// basically the same like allowClientActivation(), this time allowing
// a window to be fully raised upon its own request (XRaiseWindow),
// if refused, it will be raised only on top of windows belonging
// to the same application
template<typename Space, typename Win>
bool allow_full_window_raising(Space& space, Win const* window, xcb_timestamp_t time)
{
    auto level
        = window->control->rules.checkFSP(space.options->qobject->focusStealingPreventionLevel());
    if (space.session_manager->state() == session_state::saving
        && enum_index(level) <= enum_index(fsp_level::medium)) {
        // <= normal
        return true;
    }

    auto ac = most_recently_activated_window(space);

    if (level == fsp_level::none) {
        return true;
    }
    if (level == fsp_level::extreme) {
        return false;
    }

    if (!ac) {
        qCDebug(KWIN_CORE) << "Raising: No client active, allowing";
        // no active client -> always allow
        return true;
    }

    return std::visit(
        overload{[&](auto&& ac) {
            if (is_desktop(ac)) {
                // no active client -> always allow
                return true;
            }

            // TODO window urgency  -> return true?
            if (belong_to_same_client(window, ac, same_client_check::relaxed_for_active)) {
                qCDebug(KWIN_CORE) << "Raising: Belongs to active application";
                return true;
            }

            if (level == fsp_level::high) {
                return false;
            }

            auto const user_time = ac->userTime();
            qCDebug(KWIN_CORE) << "Raising, compared:" << time << ":" << user_time << ":"
                               << (net::timestampCompare(time, user_time) >= 0);

            // time >= user_time
            return net::timestampCompare(time, user_time) >= 0;
        }},
        *ac);
}

}
