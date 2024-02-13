/*
    SPDX-FileCopyrightText: 2010 Fredrik Höglund <fredrik@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "blur_config.h"
// KConfigSkeleton
#include "blurconfig.h"

#include <base/config-como.h>

#include <KPluginFactory>
#include <kwineffects_interface.h>

K_PLUGIN_CLASS(como::BlurEffectConfig)

namespace como
{

BlurEffectConfig::BlurEffectConfig(QObject* parent, const KPluginMetaData& data)
    : KCModule(parent, data)
{
    ui.setupUi(widget());
    BlurConfig::instance(KWIN_CONFIG);
    addConfig(BlurConfig::self(), widget());
}

BlurEffectConfig::~BlurEffectConfig()
{
}

void BlurEffectConfig::save()
{
    KCModule::save();

    OrgKdeKwinEffectsInterface interface(
        QStringLiteral("org.kde.KWin"), QStringLiteral("/Effects"), QDBusConnection::sessionBus());
    interface.reconfigureEffect(QStringLiteral("blur"));
}

}

#include "blur_config.moc"
