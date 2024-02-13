/*
SPDX-FileCopyrightText: 2007 Rivo Laks <rivolaks@hot.ee>

SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef KWIN_INVERT_CONFIG_H
#define KWIN_INVERT_CONFIG_H

#include <KCModule>

class KShortcutsEditor;

namespace como
{

class InvertEffectConfig : public KCModule
{
    Q_OBJECT
public:
    explicit InvertEffectConfig(QObject* parent, const KPluginMetaData& data);

public Q_SLOTS:
    void save() override;
    void load() override;
    void defaults() override;

private:
    KShortcutsEditor* mShortcutEditor;
};

} // namespace

#endif
