/*
SPDX-FileCopyrightText: 2007 Rivo Laks <rivolaks@hot.ee>
SPDX-FileCopyrightText: 2010 Jorge Mata <matamax123@gmail.com>

SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef KWIN_TRACKMOUSE_CONFIG_H
#define KWIN_TRACKMOUSE_CONFIG_H

#include <kcmodule.h>

#include "ui_trackmouse_config.h"

class KActionCollection;

namespace como
{

class TrackMouseEffectConfig : public KCModule
{
    Q_OBJECT
public:
    explicit TrackMouseEffectConfig(QObject* parent, const KPluginMetaData& data);
    ~TrackMouseEffectConfig() override;

public Q_SLOTS:
    void save() override;
    void load() override;
    void defaults() override;
private Q_SLOTS:
    void shortcutChanged(const QKeySequence& seq);

private:
    Ui::TrackMouseEffectConfigForm m_ui;
    KActionCollection* m_actionCollection;
};

} // namespace

#endif
