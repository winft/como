/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "contrast.h"

#include <como/render/effect/interface/effect_plugin_factory.h>

namespace como
{

KWIN_EFFECT_FACTORY_SUPPORTED_ENABLED(ContrastEffect,
                                      "metadata.json.stripped",
                                      return ContrastEffect::supported();
                                      , return ContrastEffect::enabledByDefault();)

}

#include "main.moc"
