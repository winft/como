/*
SPDX-FileCopyrightText: 2017, 2018 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef SLIDE_CONFIG_H
#define SLIDE_CONFIG_H

#include "ui_slide_config.h"
#include <KCModule>

namespace como
{

class SlideEffectConfig : public KCModule
{
    Q_OBJECT

public:
    explicit SlideEffectConfig(QObject* parent, const KPluginMetaData& data);
    ~SlideEffectConfig() override;

    void save() override;

private:
    ::Ui::SlideEffectConfig m_ui;
};

}

#endif
