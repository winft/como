/*
    SPDX-FileCopyrightText: 2022 David Edmundson <davidedmundson@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "blendchanges.h"

#include <como/render/effect/interface/effect_plugin_factory.h>

namespace como
{

KWIN_EFFECT_FACTORY_SUPPORTED(BlendChanges,
                              "metadata.json.stripped",
                              return BlendChanges::supported();)

}

#include "main.moc"
