/*
    SPDX-FileCopyrightText: 2009 Martin Gräßlin <mgraesslin@kde.org>

SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "cubeslide_config.h"
// KConfigSkeleton
#include "cubeslideconfig.h"

#include <como/base/config-como.h>
#include <kwineffects_interface.h>

#include <KPluginFactory>
#include <QVBoxLayout>
#include <kconfiggroup.h>

K_PLUGIN_CLASS(como::CubeSlideEffectConfig)

namespace como
{

CubeSlideEffectConfig::CubeSlideEffectConfig(QObject* parent, const KPluginMetaData& data)
    : KCModule(parent, data)
{
    m_ui.setupUi(widget());

    CubeSlideConfig::instance(KWIN_CONFIG);
    addConfig(CubeSlideConfig::self(), widget());

    load();
}

void CubeSlideEffectConfig::save()
{
    KCModule::save();
    OrgKdeKwinEffectsInterface interface(
        QStringLiteral("org.kde.KWin"), QStringLiteral("/Effects"), QDBusConnection::sessionBus());
    interface.reconfigureEffect(QStringLiteral("cubeslide"));
}

} // namespace

#include "cubeslide_config.moc"
