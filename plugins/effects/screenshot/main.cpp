/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "screenshot.h"

#include <como/render/effect/interface/effect_plugin_factory.h>

namespace como
{

KWIN_EFFECT_FACTORY_SUPPORTED(ScreenShotEffect,
                              "metadata.json.stripped",
                              return ScreenShotEffect::supported();)

}

#include "main.moc"
