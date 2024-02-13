/*
    SPDX-FileCopyrightText: 2023 Fushan Wen <qydwhotmail@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "colorblindnesscorrection.h"

#include <como/render/effect/interface/effect_plugin_factory.h>

namespace como
{

KWIN_EFFECT_FACTORY_SUPPORTED(ColorBlindnessCorrectionEffect,
                              "metadata.json.stripped",
                              return ColorBlindnessCorrectionEffect::supported();)

}

#include "main.moc"
