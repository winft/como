/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "overvieweffect.h"

#include <render/effect/interface/effect_plugin_factory.h>

namespace como
{

KWIN_EFFECT_FACTORY_SUPPORTED(OverviewEffect,
                              "metadata.json.stripped",
                              return OverviewEffect::supported();)

}

#include "main.moc"
