/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "highlightwindow.h"

#include <como/render/effect/interface/effect_plugin_factory.h>

namespace como
{

KWIN_EFFECT_FACTORY(HighlightWindowEffect, "metadata.json.stripped")

}

#include "main.moc"
