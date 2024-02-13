/*
SPDX-FileCopyrightText: 2017, 2018 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "slide_config.h"
// KConfigSkeleton
#include "slideconfig.h"

#include <base/config-como.h>

#include <kwineffects_interface.h>

#include <KPluginFactory>

K_PLUGIN_CLASS(como::SlideEffectConfig)

namespace como
{

SlideEffectConfig::SlideEffectConfig(QObject* parent, const KPluginMetaData& data)
    : KCModule(parent, data)
{
    m_ui.setupUi(widget());
    SlideConfig::instance(KWIN_CONFIG);
    addConfig(SlideConfig::self(), widget());
}

SlideEffectConfig::~SlideEffectConfig()
{
}

void SlideEffectConfig::save()
{
    KCModule::save();

    OrgKdeKwinEffectsInterface interface(
        QStringLiteral("org.kde.KWin"), QStringLiteral("/Effects"), QDBusConnection::sessionBus());
    interface.reconfigureEffect(QStringLiteral("slide"));
}

}

#include "slide_config.moc"
