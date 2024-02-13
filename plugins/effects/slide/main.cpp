/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "slide.h"

#include <render/effect/interface/effect_plugin_factory.h>

namespace como
{

KWIN_EFFECT_FACTORY_SUPPORTED(SlideEffect,
                              "metadata.json.stripped",
                              return SlideEffect::supported();)

}

#include "main.moc"
