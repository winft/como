/*
    SPDX-FileCopyrightText: 2023 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <como_export.h>

#include <X11/Xlib.h>

#include "base/x11/fixx11h.h"

#include <Qt>
#include <xcb/xcb.h>

namespace KWin::win::x11::key_server
{

static const int MODE_SWITCH = 0x2000;

/**
 * Test if the shift modifier should be recorded for a given key.
 *
 * For example, if shift+5 produces '%' Qt wants ctrl+shift+5 recorded as ctrl+% and
 * in that case this function would return false.
 */
COMO_EXPORT bool isShiftAsModifierAllowed(int keyQt);

/**
 * Initialises the values to return for the mod*() functions below.
 * Called automatically by those functions if not already initialized.
 */
COMO_EXPORT bool initializeMods();

/**
 * Returns true if the current keyboard layout supports the Meta key.
 * Specifically, whether the Super or Meta keys are assigned to an X modifier.
 */
COMO_EXPORT bool keyboardHasMetaKey();

/// Returns the X11 Shift modifier mask/flag.
COMO_EXPORT uint modXShift();

/// Returns the X11 Lock modifier mask/flag.
COMO_EXPORT uint modXLock();

/// Returns the X11 Ctrl modifier mask/flag.
COMO_EXPORT uint modXCtrl();

/// Returns the X11 Alt (Mod1) modifier mask/flag.
COMO_EXPORT uint modXAlt();

/// Returns the X11 Win (Mod3) modifier mask/flag.
COMO_EXPORT uint modXMeta();

/// Returns the X11 NumLock modifier mask/flag.
COMO_EXPORT uint modXNumLock();

/// Returns the X11 ScrollLock modifier mask/flag.
COMO_EXPORT uint modXScrollLock();

/// Returns the X11 Mode_switch modifier mask/flag.
COMO_EXPORT uint modXModeSwitch();

/// Returns bitwise OR'ed mask containing Shift, Ctrl, Alt, and Win (if available).
COMO_EXPORT uint accelModMaskX();

/// Extracts the symbol from the given Qt key and converts it to an X11 symbol + modifiers.
COMO_EXPORT bool keyQtToSymX(int keyQt, int* sym);

/// Extracts the code from the given Qt key.
COMO_EXPORT bool keyQtToCodeX(int keyQt, int* keyCode);

/// Extracts the modifiers from the given Qt key and converts them in a mask of X11 modifiers.
COMO_EXPORT bool keyQtToModX(int keyQt, uint* mod);

/// Converts the given symbol and modifier combination to a Qt key code.
COMO_EXPORT bool symXModXToKeyQt(uint32_t keySym, uint16_t modX, int* keyQt);

/// Converts the mask of ORed X11 modifiers to a mask of ORed Qt key code modifiers.
COMO_EXPORT bool modXToQt(uint modX, int* modQt);

/// Converts an X keypress event into a Qt key + modifier code
COMO_EXPORT bool xEventToQt(XEvent* e, int* keyModQt);

/// Converts an XCB keypress event into a Qt key + modifier code
COMO_EXPORT bool xcbKeyPressEventToQt(xcb_generic_event_t* e, int* keyModQt);
COMO_EXPORT bool xcbKeyPressEventToQt(xcb_key_press_event_t* e, int* keyModQt);

inline Qt::KeyboardModifiers to_qt_keyboard_modifiers(int state)
{
    Qt::KeyboardModifiers ret = {};

    if (state & XCB_KEY_BUT_MASK_SHIFT) {
        ret |= Qt::ShiftModifier;
    }
    if (state & XCB_KEY_BUT_MASK_CONTROL) {
        ret |= Qt::ControlModifier;
    }
    if (state & modXAlt()) {
        ret |= Qt::AltModifier;
    }
    if (state & modXMeta()) {
        ret |= Qt::MetaModifier;
    }

    return ret;
}

}
