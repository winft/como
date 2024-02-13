/*
    SPDX-FileCopyrightText: 2023 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <KSharedConfig>

namespace como::input
{

class config
{
public:
    config(KConfig::OpenFlag open_mode)
        : main{KSharedConfig::openConfig(QStringLiteral("kcminputrc"), open_mode)}
        , xkb{KSharedConfig::openConfig(QStringLiteral("kxkbrc"), open_mode)}
    {
    }

    KSharedConfigPtr main;
    KSharedConfigPtr xkb;
};

}
