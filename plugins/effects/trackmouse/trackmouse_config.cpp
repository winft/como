/*
SPDX-FileCopyrightText: 2007 Rivo Laks <rivolaks@hot.ee>
SPDX-FileCopyrightText: 2010 Jorge Mata <matamax123@gmail.com>

SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "trackmouse_config.h"

// KConfigSkeleton
#include "trackmouseconfig.h"

#include <base/config-como.h>
#include <kwineffects_interface.h>

#include <KActionCollection>
#include <KGlobalAccel>
#include <KLocalizedString>
#include <KPluginFactory>
#include <QAction>
#include <QLabel>
#include <QVBoxLayout>

K_PLUGIN_CLASS(KWin::TrackMouseEffectConfig)

namespace KWin
{

static const QString s_toggleTrackMouseActionName = QStringLiteral("TrackMouse");

TrackMouseEffectConfig::TrackMouseEffectConfig(QObject* parent, const KPluginMetaData& data)
    : KCModule(parent, data)
{
    TrackMouseConfig::instance(KWIN_CONFIG);
    m_ui.setupUi(widget());

    addConfig(TrackMouseConfig::self(), widget());

    m_actionCollection = new KActionCollection(this, QStringLiteral("kwin"));
    m_actionCollection->setComponentDisplayName(i18n("KWin"));
    m_actionCollection->setConfigGroup(QStringLiteral("TrackMouse"));
    m_actionCollection->setConfigGlobal(true);

    QAction* a = m_actionCollection->addAction(s_toggleTrackMouseActionName);
    a->setText(i18n("Track mouse"));
    a->setProperty("isConfigurationAction", true);

    KGlobalAccel::self()->setDefaultShortcut(a, QList<QKeySequence>());
    KGlobalAccel::self()->setShortcut(a, QList<QKeySequence>());

    connect(m_ui.shortcut,
            &KKeySequenceWidget::keySequenceChanged,
            this,
            &TrackMouseEffectConfig::shortcutChanged);
}

TrackMouseEffectConfig::~TrackMouseEffectConfig()
{
}

void TrackMouseEffectConfig::load()
{
    KCModule::load();

    if (QAction* a = m_actionCollection->action(s_toggleTrackMouseActionName)) {
        auto shortcuts = KGlobalAccel::self()->shortcut(a);
        if (!shortcuts.isEmpty()) {
            m_ui.shortcut->setKeySequence(shortcuts.first());
        }
    }
}

void TrackMouseEffectConfig::save()
{
    KCModule::save();
    m_actionCollection->writeSettings();
    OrgKdeKwinEffectsInterface interface(
        QStringLiteral("org.kde.KWin"), QStringLiteral("/Effects"), QDBusConnection::sessionBus());
    interface.reconfigureEffect(QStringLiteral("trackmouse"));
}

void TrackMouseEffectConfig::defaults()
{
    KCModule::defaults();
    m_ui.shortcut->clearKeySequence();
}

void TrackMouseEffectConfig::shortcutChanged(const QKeySequence& seq)
{
    if (QAction* a = m_actionCollection->action(QStringLiteral("TrackMouse"))) {
        KGlobalAccel::self()->setShortcut(
            a, QList<QKeySequence>() << seq, KGlobalAccel::NoAutoloading);
    }
    setNeedsSave(true);
}

} // namespace

#include "trackmouse_config.moc"
