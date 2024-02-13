/*
    SPDX-FileCopyrightText: 2022 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "window.h"

#include "base/logging.h"
#include "win/rules.h"

namespace como::win::rules
{

template<typename Win>
void setup_rules(Win* win)
{
    // TODO(romangg): This disconnects all connections of captionChanged to the window itself.
    //                There is only one so this works fine but it's not robustly specified.
    //                Either reshuffle later or use explicit connection object.
    QObject::disconnect(
        win->qobject.get(), &window_qobject::captionChanged, win->qobject.get(), nullptr);
    win->control->rules = find_window(*win->space.rule_book, *win);
    // check only after getting the rules, because there may be a rule forcing window type
    // TODO(romangg): what does this mean?
}

template<typename Win>
void evaluate_rules(Win* win)
{
    setup_rules(win);

    if constexpr (requires(Win win) { win.applyWindowRules(); }) {
        win->applyWindowRules();
    } else {
        apply_window_rules(*win);
    }
}

template<typename Ruling, typename RefWin>
bool match_rule(Ruling& ruling, RefWin const& ref_win)
{
    if (!ruling.matchType(ref_win.get_window_type_direct())) {
        return false;
    }
    if (!ruling.matchWMClass(ref_win.meta.wm_class.res_class, ref_win.meta.wm_class.res_name)) {
        return false;
    }
    if (!ruling.matchRole(ref_win.windowRole().toLower())) {
        return false;
    }

    if constexpr (requires(RefWin win) { win.get_client_machine(); }) {
        if (auto cm = ref_win.get_client_machine();
            cm && !ruling.matchClientMachine(cm->hostname(), cm->is_local())) {
            return false;
        }
    }

    if (ruling.title.match != name_match::unimportant) {
        // Track title changes to rematch rules.
        auto mutable_client = const_cast<RefWin*>(&ref_win);
        QObject::connect(
            mutable_client->qobject.get(),
            &RefWin::qobject_t::captionChanged,
            mutable_client->qobject.get(),
            [mutable_client] { evaluate_rules(mutable_client); },
            // QueuedConnection, because title may change before
            // the client is ready (could segfault!)
            static_cast<Qt::ConnectionType>(Qt::QueuedConnection | Qt::UniqueConnection));
    }

    return ruling.matchTitle(ref_win.meta.caption.normal);
}

template<typename Book, typename RefWin>
window find_window(Book& book, RefWin& ref_win)
{
    std::vector<ruling*> ret;

    for (auto it = book.m_rules.begin(); it != book.m_rules.end();) {
        if (match_rule(**it, ref_win)) {
            auto rule = *it;
            qCDebug(KWIN_CORE) << "Rule found:" << rule << ":" << &ref_win;
            ++it;
            ret.push_back(rule);
            continue;
        }
        ++it;
    }

    return rules::window(ret);
}

}
