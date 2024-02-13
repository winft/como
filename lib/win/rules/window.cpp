/*
    SPDX-FileCopyrightText: 2004 Lubos Lunak <l.lunak@kde.org>
    SPDX-FileCopyrightText: 2020 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "window.h"

#include <kconfig.h>

#include "win/controlling.h"
#include "win/geo_change.h"
#include "win/input.h"
#include "win/meta.h"
#include "win/rules.h"
#include "win/screen.h"
#include "win/stacking.h"
#include "win/x11/client_machine.h"

#include "book.h"
#include "book_settings.h"
#include "rules_settings.h"
#include "update.h"

namespace como::win::rules
{

window::window(std::vector<ruling*> const& rules)
    : rules{rules}
{
}

window::window()
{
}

bool window::contains(ruling const* rule) const
{
    return como::contains(rules, const_cast<ruling*>(rule));
}

void window::remove(ruling* rule)
{
    remove_all(rules, rule);
}

QRect window::checkGeometry(QRect rect, bool init) const
{
    return QRect(checkPosition(rect.topLeft(), init), checkSize(rect.size(), init));
}

QPoint window::checkPosition(QPoint pos, bool init) const
{
    return check_set(pos, init, &ruling::applyPosition);
}

QSize window::checkSize(QSize s, bool init) const
{
    return check_set(s, init, &ruling::applySize);
}

bool window::checkIgnoreGeometry(bool ignore, bool init) const
{
    return check_set(ignore, init, &ruling::applyIgnoreGeometry);
}

maximize_mode window::checkMaximizeVert(maximize_mode mode, bool init) const
{
    return check_set(mode, init, &ruling::applyMaximizeVert);
}

maximize_mode window::checkMaximizeHoriz(maximize_mode mode, bool init) const
{
    return check_set(mode, init, &ruling::applyMaximizeHoriz);
}

bool window::checkMinimize(bool minimized, bool init) const
{
    return check_set(minimized, init, &ruling::applyMinimize);
}

bool window::checkSkipTaskbar(bool skip, bool init) const
{
    return check_set(skip, init, &ruling::applySkipTaskbar);
}

bool window::checkSkipPager(bool skip, bool init) const
{
    return check_set(skip, init, &ruling::applySkipPager);
}

bool window::checkSkipSwitcher(bool skip, bool init) const
{
    return check_set(skip, init, &ruling::applySkipSwitcher);
}

bool window::checkKeepAbove(bool above, bool init) const
{
    return check_set(above, init, &ruling::applyKeepAbove);
}

bool window::checkKeepBelow(bool below, bool init) const
{
    return check_set(below, init, &ruling::applyKeepBelow);
}

bool window::checkFullScreen(bool fs, bool init) const
{
    return check_set(fs, init, &ruling::applyFullScreen);
}

bool window::checkNoBorder(bool noborder, bool init) const
{
    return check_set(noborder, init, &ruling::applyNoBorder);
}

QString window::checkShortcut(QString s, bool init) const
{
    return check_set(s, init, &ruling::applyShortcut);
}

QString window::checkDesktopFile(QString desktopFile, bool init) const
{
    return check_set(desktopFile, init, &ruling::applyDesktopFile);
}

placement window::checkPlacement(placement placement) const
{
    return check_force(placement, &ruling::applyPlacement);
}

QSize window::checkMinSize(QSize s) const
{
    return check_force(s, &ruling::applyMinSize);
}

QSize window::checkMaxSize(QSize s) const
{
    return check_force(s, &ruling::applyMaxSize);
}

int window::checkOpacityActive(int s) const
{
    return check_force(s, &ruling::applyOpacityActive);
}

int window::checkOpacityInactive(int s) const
{
    return check_force(s, &ruling::applyOpacityInactive);
}

win_type window::checkType(win_type type) const
{
    return check_force(type, &ruling::applyType);
}

QString window::checkDecoColor(QString schemeFile) const
{
    return check_force(schemeFile, &ruling::applyDecoColor);
}

bool window::checkBlockCompositing(bool block) const
{
    return check_force(block, &ruling::applyBlockCompositing);
}

fsp_level window::checkFSP(fsp_level fsp) const
{
    return check_force(fsp, &ruling::applyFSP);
}

fsp_level window::checkFPP(fsp_level fpp) const
{
    return check_force(fpp, &ruling::applyFPP);
}

bool window::checkAcceptFocus(bool focus) const
{
    return check_force(focus, &ruling::applyAcceptFocus);
}

bool window::checkCloseable(bool closeable) const
{
    return check_force(closeable, &ruling::applyCloseable);
}

bool window::checkAutogrouping(bool autogroup) const
{
    return check_force(autogroup, &ruling::applyAutogrouping);
}

bool window::checkAutogroupInForeground(bool fg) const
{
    return check_force(fg, &ruling::applyAutogroupInForeground);
}

QString window::checkAutogroupById(QString id) const
{
    return check_force(id, &ruling::applyAutogroupById);
}

bool window::checkStrictGeometry(bool strict) const
{
    return check_force(strict, &ruling::applyStrictGeometry);
}

bool window::checkDisableGlobalShortcuts(bool disable) const
{
    return check_force(disable, &ruling::applyDisableGlobalShortcuts);
}

maximize_mode window::checkMaximize(maximize_mode mode, bool init) const
{
    auto vert = checkMaximizeVert(mode, init) & maximize_mode::vertical;
    auto horiz = checkMaximizeHoriz(mode, init) & maximize_mode::horizontal;
    return vert | horiz;
}

}
