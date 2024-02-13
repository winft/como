/*
    SPDX-FileCopyrightText: 2020 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "key_server.h"

#include "base/options.h"

namespace como::win::x11
{

template<typename Win>
static inline uint16_t x11CommandAllModifier(Win& win)
{
    switch (win.space.options->qobject->commandAllModifier()) {
    case Qt::MetaModifier:
        return key_server::modXMeta();
    case Qt::AltModifier:
        return key_server::modXAlt();
    default:
        return 0;
    }
}

#define XCapL key_server::modXLock()
#define XNumL key_server::modXNumLock()
#define XScrL key_server::modXScrollLock()

template<typename Win>
void establish_command_window_grab(Win* win, uint8_t button)
{
    // Unfortunately there are a lot of possible modifier combinations that we need to take into
    // account. We tackle that problem in a kind of smart way. First, we grab the button with all
    // possible modifiers, then we ungrab the ones that are relevant only to commandAllx().

    win->xcb_windows.wrapper.grab_button(
        XCB_GRAB_MODE_SYNC, XCB_GRAB_MODE_ASYNC, XCB_MOD_MASK_ANY, button);

    auto x11Modifier = x11CommandAllModifier(*win);

    unsigned int mods[8] = {
        0, XCapL, XNumL, XNumL | XCapL, XScrL, XScrL | XCapL, XScrL | XNumL, XScrL | XNumL | XCapL};
    for (int i = 0; i < 8; ++i)
        win->xcb_windows.wrapper.ungrab_button(x11Modifier | mods[i], button);
}

template<typename Win>
void establish_command_all_grab(Win* win, uint8_t button)
{
    uint16_t x11Modifier = x11CommandAllModifier(*win);

    unsigned int mods[8] = {
        0, XCapL, XNumL, XNumL | XCapL, XScrL, XScrL | XCapL, XScrL | XNumL, XScrL | XNumL | XCapL};
    for (int i = 0; i < 8; ++i)
        win->xcb_windows.wrapper.grab_button(
            XCB_GRAB_MODE_SYNC, XCB_GRAB_MODE_ASYNC, x11Modifier | mods[i], button);
}
#undef XCapL
#undef XNumL
#undef XScrL

}
