/*
SPDX-FileCopyrightText: 2012 Filip Wieladek <wattos@gmail.com>

SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "mouseclick_config.h"

// KConfigSkeleton
#include "mouseclickconfig.h"

#include <como/base/config-como.h>
#include <kwineffects_interface.h>

#include <KActionCollection>
#include <KGlobalAccel>
#include <KLocalizedString>
#include <KPluginFactory>
#include <QAction>
#include <QWidget>

K_PLUGIN_CLASS(como::MouseClickEffectConfig)

namespace como
{

MouseClickEffectConfig::MouseClickEffectConfig(QObject* parent, const KPluginMetaData& data)
    : KCModule(parent, data)
{
    m_ui.setupUi(widget());

    connect(m_ui.editor, &KShortcutsEditor::keyChange, this, &KCModule::markAsChanged);

    // Shortcut config. The shortcut belongs to the component "kwin"!
    m_actionCollection = new KActionCollection(this, QStringLiteral("kwin"));
    m_actionCollection->setComponentDisplayName(i18n("KWin"));

    QAction* a = m_actionCollection->addAction(QStringLiteral("ToggleMouseClick"));
    a->setText(i18n("Toggle Mouse Click Effect"));
    a->setProperty("isConfigurationAction", true);
    KGlobalAccel::self()->setDefaultShortcut(
        a, QList<QKeySequence>() << static_cast<Qt::Key>(Qt::META) + Qt::Key_Asterisk);
    KGlobalAccel::self()->setShortcut(
        a, QList<QKeySequence>() << static_cast<Qt::Key>(Qt::META) + Qt::Key_Asterisk);

    m_ui.editor->addCollection(m_actionCollection);

    MouseClickConfig::instance(KWIN_CONFIG);
    addConfig(MouseClickConfig::self(), widget());
}

void MouseClickEffectConfig::save()
{
    KCModule::save();
    m_ui.editor->save(); // undo() will restore to this state from now on
    OrgKdeKwinEffectsInterface interface(
        QStringLiteral("org.kde.KWin"), QStringLiteral("/Effects"), QDBusConnection::sessionBus());
    interface.reconfigureEffect(QStringLiteral("mouseclick"));
}

} // namespace

#include "mouseclick_config.moc"
