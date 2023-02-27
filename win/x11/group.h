/*
SPDX-FileCopyrightText: 1999, 2000 Matthias Ettrich <ettrich@kde.org>
SPDX-FileCopyrightText: 2003 Lubos Lunak <l.lunak@kde.org>

SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "extras.h"
#include "startup_notify.h"
#include "window_find.h"

#include "render/effect/window_group_impl.h"

#include <QIcon>
#include <vector>

namespace KWin::win::x11
{

template<typename Space>
class group
{
public:
    using group_t = group<Space>;
    using x11_window_t = typename Space::x11_window;

    group(xcb_window_t xcb_leader, Space& space)
        : xcb_leader{xcb_leader}
        , space{space}
    {
        if (xcb_leader != XCB_WINDOW_NONE) {
            leader
                = find_controlled_window<x11_window_t>(space, predicate_match::window, xcb_leader);
            leader_info = new net::win_info(space.base.x11_data.connection,
                                            xcb_leader,
                                            space.base.x11_data.root_window,
                                            net::Properties(),
                                            net::WM2StartupId);
        }
        effect_group = new render::effect_window_group_impl<group_t>(this);
        space.groups.push_back(this);
    }

    ~group()
    {
        remove_all(space.groups, this);
        delete leader_info;
        delete effect_group;
    }

    QIcon icon() const
    {
        if (leader) {
            return leader->control->icon;
        } else if (xcb_leader != XCB_WINDOW_NONE) {
            QIcon ic;
            net::win_info info(space.base.x11_data.connection,
                               xcb_leader,
                               space.base.x11_data.root_window,
                               net::WMIcon,
                               net::WM2IconPixmap);
            auto readIcon = [&ic, &info, this](int size, bool scale = true) {
                auto const pix = extras::icon(
                    xcb_leader, size, size, scale, extras::NETWM | extras::WMHints, &info);
                if (!pix.isNull()) {
                    ic.addPixmap(pix);
                }
            };
            readIcon(16);
            readIcon(32);
            readIcon(48, false);
            readIcon(64, false);
            readIcon(128, false);
            return ic;
        }
        return QIcon();
    }

    void addMember(x11_window_t* member)
    {
        members.push_back(member);
    }

    void removeMember(x11_window_t* member)
    {
        assert(std::find(members.cbegin(), members.cend(), member) != members.cend());
        remove_all(members, member);
        // there are cases when automatic deleting of groups must be delayed,
        // e.g. when removing a member and doing some operation on the possibly
        // other members of the group (which would be however deleted already
        // if there were no other members)
        if (refcount == 0 && members.empty()) {
            delete this;
        }
    }

    void gotLeader(x11_window_t* leader)
    {
        assert(leader->xcb_windows.client == xcb_leader);
        this->leader = leader;
    }

    void lostLeader()
    {
        assert(std::find(members.cbegin(), members.cend(), leader) == members.cend());
        leader = nullptr;
        if (members.empty()) {
            delete this;
        }
    }

    void updateUserTime(xcb_timestamp_t time)
    {
        // copy of win::x11::update_user_time in control.h
        if (time == XCB_CURRENT_TIME) {
            base::x11::update_time_from_clock(space.base);
            time = space.base.x11_data.time;
        }
        if (time != -1U
            && (user_time == XCB_CURRENT_TIME
                || net::timestampCompare(time, user_time) > 0)) // time > user_time
            user_time = time;
    }

    void ref()
    {
        ++refcount;
    }

    void deref()
    {
        if (--refcount == 0 && members.empty()) {
            delete this;
        }
    }

    std::vector<x11_window_t*> members;
    x11_window_t* leader{nullptr};
    xcb_window_t xcb_leader;
    net::win_info* leader_info{nullptr};
    xcb_timestamp_t user_time{-1U};
    render::effect_window_group_impl<group_t>* effect_group;

private:
    void startupIdChanged()
    {
        startup_info_id asn_id;
        startup_info_data asn_data;
        auto asn_valid = check_startup_notification(space, xcb_leader, asn_id, asn_data);
        if (!asn_valid) {
            return;
        }

        if (asn_id.timestamp() != 0 && user_time != -1U
            && net::timestampCompare(asn_id.timestamp(), user_time) > 0) {
            user_time = asn_id.timestamp();
        }
    }

    int refcount{0};
    Space& space;
};

template<typename Space>
group<Space>* find_group(Space& space, xcb_window_t leader)
{
    assert(leader != XCB_WINDOW_NONE);
    for (auto group : space.groups) {
        if (group->xcb_leader == leader) {
            return group;
        }
    }
    return nullptr;
}

}
