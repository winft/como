/*
    SPDX-FileCopyrightText: 2022 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <como/win/types.h>

#include <como/render/effect/interface/effect_window.h>

namespace como::render
{

template<typename Group>
class effect_window_group_impl : public EffectWindowGroup
{
public:
    explicit effect_window_group_impl(Group* group)
        : group(group)
    {
    }

    QList<EffectWindow*> members() const override
    {
        const auto memberList = group->members;
        QList<EffectWindow*> ret;
        ret.reserve(memberList.size());
        std::transform(std::cbegin(memberList),
                       std::cend(memberList),
                       std::back_inserter(ret),
                       [](auto win) { return win->render->effect.get(); });
        return ret;
    }

private:
    Group* group;
};

}
