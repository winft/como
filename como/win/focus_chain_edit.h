/*
    SPDX-FileCopyrightText: 2022 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "desktop_get.h"
#include "net.h"
#include "types.h"
#include "util.h"

#include <como/utils/algorithm.h>

#include <optional>

namespace como::win
{

template<typename Manager, typename Win>
void focus_chain_remove(Manager& manager, Win* window)
{
    using var_win = typename Win::space_t::window_t;
    for (auto& [key, chain] : manager.chains.subspaces) {
        remove_all(chain, var_win(window));
    }
    remove_all(manager.chains.latest_use, var_win(window));
}

/**
 * @brief Resizes the per subspace focus chains from @p prev_size to @p next_size.
 *
 * This means that for each subspace between previous and new size a new focus chain is
 * created and in case the number is reduced the focus chains are destroyed.
 *
 * @param prev_size The previous number of virtual subspaces
 * @param next_size The new number of virtual subspaces
 * @return void
 */
template<typename Manager>
void focus_chain_resize(Manager& manager, unsigned int prev_size, unsigned int next_size)
{
    for (auto i = prev_size + 1; i <= next_size; ++i) {
        manager.chains.subspaces.insert({i, decltype(manager.chains.latest_use)()});
    }
    for (auto i = prev_size; i > next_size; --i) {
        manager.chains.subspaces.erase(i);
    }
}

/**
 * Checks whether the focus chain for the given @p subspace contains the given @p window.
 * Does not consider the most recently used focus chain.
 */
template<typename Manager, typename VarWin>
bool focus_chain_at_subspace_contains(Manager& manager, VarWin window, unsigned int subspace)
{
    auto it = manager.chains.subspaces.find(subspace);
    if (it == manager.chains.subspaces.end()) {
        return false;
    }
    return contains(it->second, window);
}

template<typename Win, typename VarWin, typename Chain>
void focus_chain_insert_window_into_chain(Win* window, Chain& chain, VarWin const& active_window)
{
    if (contains(chain, VarWin(window))) {
        // TODO(romangg): better assert?
        return;
    }
    if (active_window && active_window != VarWin(window) && !chain.empty()
        && VarWin(chain.back()) == active_window) {
        // Add it after the active client
        chain.insert(std::prev(chain.end()), window);
    } else {
        // Otherwise add as the first one
        chain.push_back(window);
    }
}

template<typename Win, typename Chain>
void focus_chain_make_first_in_chain(Win* window, Chain& chain)
{
    using var_win = typename Win::space_t::window_t;
    remove_all(chain, var_win(window));
    chain.push_back(var_win(window));
}

template<typename Win, typename Chain>
void focus_chain_make_last_in_chain(Win* window, Chain& chain)
{
    using var_win = typename Win::space_t::window_t;
    remove_all(chain, var_win(window));
    chain.push_front(var_win(window));
}

template<typename Win, typename VarWin, typename Chain>
void focus_chain_update_window_in_chain(Win* window,
                                        focus_chain_change change,
                                        Chain& chain,
                                        VarWin const& active_window)
{
    if (change == focus_chain_change::make_first) {
        focus_chain_make_first_in_chain(window, chain);
    } else if (change == focus_chain_change::make_last) {
        focus_chain_make_last_in_chain(window, chain);
    } else {
        focus_chain_insert_window_into_chain(window, chain, active_window);
    }
}

/**
 * @brief Updates the position of the @p window according to the requested @p change in the
 * focus chain.
 *
 * This method affects both the most recently used focus chain and the per subspace focus chain.
 *
 * In case the client does no longer want to get focus, it is removed from all chains. In case
 * the client is on all virtual subspaces it is ensured that it is present in each of the virtual
 * subspaces focus chain. In case it's on exactly one subspace it is ensured that it is
 * only in the focus chain for that subspace.
 *
 * Depending on @p change the window is inserted at different positions in the focus chain. In
 * case of @c focus_chain_change::make_first it is moved to the first position of the chain, in case
 * of @c focus_chain_change::make_last it is moved to the last position of the chain. In all other
 * cases it depends on whether the @p window is the currently active window. If it is the active
 * window it becomes the first Client in the chain, otherwise it is inserted at the second position
 * that is directly after the currently active window.
 *
 * @param window The window which should be moved inside the chains.
 * @param change Where to move the window
 */
template<typename Manager, typename Win>
void focus_chain_update(Manager& manager, Win* window, focus_chain_change change)
{
    using var_win = typename Win::space_t::window_t;

    if (!wants_tab_focus(window)) {
        // Doesn't want tab focus, remove.
        focus_chain_remove(manager, window);
        return;
    }

    if (on_all_subspaces(*window)) {
        // Now on all subspaces, add it to focus chains it is not already in.
        for (auto& [key, chain] : manager.chains.subspaces) {
            // Making first/last works only on current subspace, don't affect all subspaces
            if (key == manager.current_subspace
                && (change == focus_chain_change::make_first
                    || change == focus_chain_change::make_last)) {
                if (change == focus_chain_change::make_first) {
                    focus_chain_make_first_in_chain(window, chain);
                } else {
                    focus_chain_make_last_in_chain(window, chain);
                }
            } else {
                focus_chain_insert_window_into_chain(window, chain, manager.active_window);
            }
        }
    } else {
        // Now only on subspace, remove it anywhere else
        for (auto& [key, chain] : manager.chains.subspaces) {
            if (on_subspace(*window, key)) {
                focus_chain_update_window_in_chain(window, change, chain, manager.active_window);
            } else {
                remove_all(chain, var_win(window));
            }
        }
    }

    // add for most recently used chain
    focus_chain_update_window_in_chain(
        window, change, manager.chains.latest_use, manager.active_window);
}

/**
 * @brief Returns the first window in the most recently used focus chain. First window in this
 * case means really the first window in the chain and not the most recently used window.
 *
 * @return The first window in the most recently used chain.
 */
template<typename Win, typename Manager>
Win focus_chain_first_latest_use(Manager& manager)
{
    auto& latest_chain = manager.chains.latest_use;
    if (latest_chain.empty()) {
        return {};
    }

    return latest_chain.front();
}

/**
 * @brief Queries the most recently used focus chain for the next window after the given
 * @p reference.
 *
 * The navigation wraps around the borders of the chain. That is if the @p reference window is
 * the last item of the focus chain, the first window will be returned.
 *
 * If the @p reference window cannot be found in the focus chain, the first element of the focus
 * chain is returned.
 *
 * @param reference The start point in the focus chain to search
 * @return The relatively next window in the most recently used chain.
 */
template<typename Manager, typename VarWin>
std::optional<VarWin> focus_chain_next_latest_use(Manager& manager, VarWin const& reference)
{
    auto& latest_chain = manager.chains.latest_use;
    if (latest_chain.empty()) {
        return {};
    }

    auto it = find(latest_chain, reference);

    if (it == latest_chain.end()) {
        return latest_chain.front();
    }
    if (it == latest_chain.begin()) {
        return latest_chain.back();
    }

    return *std::prev(it);
}

template<typename Chain, typename Win, typename RefWin>
void focus_chain_move_window_after_in_chain(Chain& chain, Win* window, RefWin* reference)
{
    using var_win = typename Win::space_t::window_t;

    if (!contains(chain, var_win(reference))) {
        // TODO(romangg): better assert?
        return;
    }

    remove_all(chain, var_win(window));

    if (belong_to_same_client(reference, window)) {
        // Simple case, just put it directly behind the reference window of the same client.
        // TODO(romangg): can this special case be explained better?
        auto it = find(chain, var_win(reference));
        chain.insert(it, window);
        return;
    }

    for (auto it = chain.rbegin(); it != chain.rend(); ++it) {
        if (std::visit(overload{[&](auto&& win) {
                           if (belong_to_same_client(reference, win)) {
                               chain.insert(std::next(it).base(), window);
                               return true;
                           }
                           return false;
                       }},
                       *it)) {
            break;
        }
    }
}

/**
 * @brief Moves @p window behind the @p reference in all focus chains.
 *
 * @param client The Client to move in the chains
 * @param reference The Client behind which the @p client should be moved
 * @return void
 */
template<typename Manager, typename Win, typename RefWin>
void focus_chain_move_window_after(Manager& manager, Win* window, RefWin* reference)
{
    if (!wants_tab_focus(window)) {
        return;
    }

    for (auto& [key, chain] : manager.chains.subspaces) {
        if (!on_subspace(*window, key)) {
            continue;
        }
        focus_chain_move_window_after_in_chain(chain, window, reference);
    }

    focus_chain_move_window_after_in_chain(manager.chains.latest_use, window, reference);
}

}
