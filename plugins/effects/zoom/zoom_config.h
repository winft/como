/*
SPDX-FileCopyrightText: 2007 Rivo Laks <rivolaks@hot.ee>
SPDX-FileCopyrightText: 2010 Sebastian Sauer <sebsauer@kdab.com>

SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef KWIN_ZOOM_CONFIG_H
#define KWIN_ZOOM_CONFIG_H

#include <KCModule>

#include "ui_zoom_config.h"

namespace como
{

class ZoomEffectConfig : public KCModule
{
    Q_OBJECT
public:
    explicit ZoomEffectConfig(QObject* parent, const KPluginMetaData& data);

public Q_SLOTS:
    void save() override;

private:
    Ui::ZoomEffectConfigForm m_ui;
    enum MouseTracking { MouseCentred = 0, MouseProportional = 1, MouseDisabled = 2 };
};

} // namespace

#endif
