/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "mouseclick.h"

#include <render/effect/interface/effect_plugin_factory.h>

namespace como
{

KWIN_EFFECT_FACTORY(MouseClickEffect, "metadata.json.stripped")

}

#include "main.moc"
