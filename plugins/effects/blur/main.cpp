/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "blur.h"

#include <render/effect/interface/effect_plugin_factory.h>

namespace como
{

KWIN_EFFECT_FACTORY_SUPPORTED_ENABLED(BlurEffect,
                                      "metadata.json.stripped",
                                      return BlurEffect::supported();
                                      , return BlurEffect::enabledByDefault();)

}

#include "main.moc"
