/*
    SPDX-FileCopyrightText: 2022 Roman Gilg <subdiff@gmail.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "cubeslide.h"

#include <como/render/effect/interface/effect_plugin_factory.h>

namespace como
{

KWIN_EFFECT_FACTORY_SUPPORTED(CubeSlideEffect,
                              "metadata.json",
                              return CubeSlideEffect::supported();)

}

#include "main.moc"
