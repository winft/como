/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <KCModule>

#include "ui_windowvieweffectkcm.h"

namespace como
{

class WindowViewEffectConfig : public KCModule
{
    Q_OBJECT

public:
    explicit WindowViewEffectConfig(QObject* parent, const KPluginMetaData& data);

public Q_SLOTS:
    void save() override;
    void defaults() override;

private:
    ::Ui::WindowViewEffectConfig ui;
};

}
