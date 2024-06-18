/*
    SPDX-FileCopyrightText: 2022 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "config-como.h"

#include <KLocalizedString>
#include <QAction>

namespace como::render::post
{

template<typename Input, typename NightColor>
void init_night_color_shortcuts(Input& input, NightColor& manager)
{
    auto toggleAction = new QAction(manager.qobject.get());
    toggleAction->setProperty("componentName", "kwin");
    toggleAction->setObjectName(QStringLiteral("Toggle Night Color"));
    toggleAction->setText(
        i18nc("Temporarily disable/reenable Night Light", "Suspend/Resume Night Light"));

    input.shortcuts->register_keyboard_default_shortcut(toggleAction, {});
    input.shortcuts->register_keyboard_shortcut(toggleAction, {});
    input.registerShortcut(
        QKeySequence(), toggleAction, manager.qobject.get(), [&manager] { manager.toggle(); });
}

}
