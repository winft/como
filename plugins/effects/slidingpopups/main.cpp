/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "slidingpopups.h"

#include <render/effect/interface/effect_plugin_factory.h>

namespace como
{

KWIN_EFFECT_FACTORY_SUPPORTED(SlidingPopupsEffect,
                              "metadata.json.stripped",
                              return SlidingPopupsEffect::supported();)

}

#include "main.moc"
