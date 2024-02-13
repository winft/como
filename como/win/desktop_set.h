/*
    SPDX-FileCopyrightText: 2022 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "desktop_get.h"
#include "focus_chain_edit.h"
#include "stacking.h"
#include "transient.h"
#include <como/win/subspace.h>

namespace como::win
{

template<typename Win>
void propagate_on_all_subspaces_to_children(Win& window);

template<typename Win>
void set_subspaces(Win& win, std::vector<subspace*> subs)
{
    // On x11 we can have only one subspace at a time.
    if (win.space.base.operation_mode == base::operation_mode::x11 && subs.size() > 1) {
        subs = {subs.back()};
    }

    subs = win.control->rules.checkDesktops(*win.space.subspace_manager, subs);

    if (subs == win.topo.subspaces) {
        return;
    }

    auto const was_on_all_subspaces = on_all_subspaces(win);
    win.topo.subspaces = subs;
    win.control->set_subspaces(subs);

    if (was_on_all_subspaces != on_all_subspaces(win)) {
        propagate_on_all_subspaces_to_children(win);
    }

    auto transients_stacking_order
        = restacked_by_space_stacking_order(win.space, win.transient->children);
    for (auto const& child : transients_stacking_order) {
        if (!child->transient->annexed) {
            set_subspaces(*child, subs);
        }
    }

    if (win.transient->modal()) {
        // When a modal dialog is moved move the parent window with it as otherwise the just moved
        // modal dialog will return to the parent window with the next desktop change.
        for (auto client : win.transient->leads()) {
            set_subspaces(*client, subs);
        }
    }

    if constexpr (requires(Win win) { win.do_set_subspace(); }) {
        // TODO(romangg): Only relevant for X11/Xwl windows. Should go somewhere else.
        win.do_set_subspace();
    }

    focus_chain_update(win.space.stacking.focus_chain, &win, focus_chain_change::make_first);
    win.updateWindowRules(rules::type::desktops);

    Q_EMIT win.qobject->subspaces_changed();
}

template<typename Win>
void set_subspace(Win& win, int subspace)
{
    if (subspace == x11_desktop_number_on_all) {
        set_subspaces(win, {});
        return;
    }

    // Check range.
    subspace = std::clamp<int>(subspace, 1, win.space.subspace_manager->subspaces.size());
    set_subspaces(win, {subspaces_get_for_x11id(*win.space.subspace_manager, subspace)});
}

template<typename Win>
void set_on_all_subspaces(Win& win, bool set)
{
    if (set == on_all_subspaces(win)) {
        return;
    }

    if (set) {
        set_subspaces(win, {});
    } else {
        set_subspaces(win, {win.space.subspace_manager->current});
    }
}

template<typename Win>
void enter_subspace(Win& win, subspace* sub)
{
    if (contains(win.topo.subspaces, sub)) {
        return;
    }

    auto subspaces = win.topo.subspaces;
    subspaces.push_back(sub);
    set_subspaces(win, subspaces);
}

template<typename Win>
void leave_subspace(Win& win, subspace* sub)
{
    auto subspaces
        = win.topo.subspaces.empty() ? win.space.subspace_manager->subspaces : win.topo.subspaces;

    auto it = std::ranges::find(subspaces, sub);
    if (it == subspaces.end()) {
        return;
    }

    subspaces.erase(it);
    set_subspaces(win, subspaces);
}

template<typename Win>
void propagate_on_all_subspaces_to_children(Win& window)
{
    auto all_desk = on_all_subspaces(window);

    for (auto const& child : window.transient->children) {
        if (on_all_subspaces(*child) != all_desk) {
            set_on_all_subspaces(*child, all_desk);
        }
    }
}

}
