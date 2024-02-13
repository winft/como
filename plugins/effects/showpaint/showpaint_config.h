/*
SPDX-FileCopyrightText: 2018 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <KCModule>

#include "ui_showpaint_config.h"

namespace como
{

class ShowPaintEffectConfig : public KCModule
{
    Q_OBJECT

public:
    explicit ShowPaintEffectConfig(QObject* parent, const KPluginMetaData& data);

public Q_SLOTS:
    void save() override;
    void defaults() override;

private:
    Ui::ShowPaintEffectConfig m_ui;
};

}
