/*
    SPDX-FileCopyrightText: 2011 Arthur Arlt <a.arlt@stud.uni-heidelberg.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <como/base/x11/data.h>
#include <como/base/x11/xcb/window.h>
#include <como/render/outline.h>
#include <como_export.h>

namespace como::render::backend::x11
{

class COMO_EXPORT non_composited_outline : public outline_visual
{
public:
    non_composited_outline(base::x11::data const& data, render::outline* outline);
    ~non_composited_outline() override;
    void show() override;
    void hide() override;

private:
    // TODO: variadic template arguments for adding method arguments
    template<typename T>
    void forEachWindow(T method);
    bool m_initialized{false};
    base::x11::xcb::window m_topOutline;
    base::x11::xcb::window m_rightOutline;
    base::x11::xcb::window m_bottomOutline;
    base::x11::xcb::window m_leftOutline;
    base::x11::data const& data;
};

template<typename T>
inline void non_composited_outline::forEachWindow(T method)
{
    (m_topOutline.*method)();
    (m_rightOutline.*method)();
    (m_bottomOutline.*method)();
    (m_leftOutline.*method)();
}

}
