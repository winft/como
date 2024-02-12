/*
SPDX-FileCopyrightText: 2007 Christian Nitschkowski <christian.nitschkowski@kdemail.net>

SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "mousemark_config.h"

// KConfigSkeleton
#include "mousemarkconfig.h"

#include <base/config-como.h>
#include <kwineffects_interface.h>

#include <QAction>

#include <KActionCollection>
#include <KGlobalAccel>
#include <KLocalizedString>
#include <KPluginFactory>
#include <QDebug>
#include <QWidget>

K_PLUGIN_CLASS(KWin::MouseMarkEffectConfig)

namespace KWin
{

MouseMarkEffectConfig::MouseMarkEffectConfig(QObject* parent, const KPluginMetaData& data)
    : KCModule(parent, data)
{
    m_ui.setupUi(widget());

    MouseMarkConfig::instance(KWIN_CONFIG);
    addConfig(MouseMarkConfig::self(), widget());

    // Shortcut config. The shortcut belongs to the component "kwin"!
    m_actionCollection = new KActionCollection(this, QStringLiteral("kwin"));
    m_actionCollection->setComponentDisplayName(i18n("KWin"));

    QAction* a = m_actionCollection->addAction(QStringLiteral("ClearMouseMarks"));
    a->setText(i18n("Clear Mouse Marks"));
    a->setProperty("isConfigurationAction", true);
    KGlobalAccel::self()->setDefaultShortcut(
        a, QList<QKeySequence>() << (Qt::SHIFT | Qt::META | Qt::Key_F11));
    KGlobalAccel::self()->setShortcut(
        a, QList<QKeySequence>() << (Qt::SHIFT | Qt::META | Qt::Key_F11));

    a = m_actionCollection->addAction(QStringLiteral("ClearLastMouseMark"));
    a->setText(i18n("Clear Last Mouse Mark"));
    a->setProperty("isConfigurationAction", true);
    KGlobalAccel::self()->setDefaultShortcut(
        a, QList<QKeySequence>() << (Qt::SHIFT | Qt::META | Qt::Key_F12));
    KGlobalAccel::self()->setShortcut(
        a, QList<QKeySequence>() << (Qt::SHIFT | Qt::META | Qt::Key_F12));

    m_ui.editor->addCollection(m_actionCollection);

    connect(m_ui.kcfg_LineWidth, qOverload<int>(&QSpinBox::valueChanged), this, [this]() {
        updateSpinBoxSuffix();
    });
}

void MouseMarkEffectConfig::load()
{
    KCModule::load();

    updateSpinBoxSuffix();
}

void MouseMarkEffectConfig::save()
{
    qDebug() << "Saving config of MouseMark";
    KCModule::save();

    m_actionCollection->writeSettings();
    m_ui.editor->save(); // undo() will restore to this state from now on

    OrgKdeKwinEffectsInterface interface(
        QStringLiteral("org.kde.KWin"), QStringLiteral("/Effects"), QDBusConnection::sessionBus());
    interface.reconfigureEffect(QStringLiteral("mousemark"));
}

void MouseMarkEffectConfig::updateSpinBoxSuffix()
{
    m_ui.kcfg_LineWidth->setSuffix(
        i18ncp("Suffix", " pixel", " pixels", m_ui.kcfg_LineWidth->value()));
}

} // namespace

#include "mousemark_config.moc"
