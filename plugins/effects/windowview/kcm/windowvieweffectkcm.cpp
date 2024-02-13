/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>
    SPDX-FileCopyrightText: 2022 ivan tkachenko <me@ratijas.tk>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "windowvieweffectkcm.h"

#include "windowviewconfig.h"

#include <base/config-como.h>
#include <kwineffects_interface.h>

#include <KActionCollection>
#include <KGlobalAccel>
#include <KLocalizedString>
#include <KPluginFactory>

#include <QAction>

K_PLUGIN_CLASS(como::WindowViewEffectConfig)

namespace como
{

WindowViewEffectConfig::WindowViewEffectConfig(QObject* parent, const KPluginMetaData& data)
    : KCModule(parent, data)
{
    ui.setupUi(widget());
    WindowViewConfig::instance(KWIN_CONFIG);
    addConfig(WindowViewConfig::self(), widget());

    auto actionCollection = new KActionCollection(widget(), QStringLiteral("kwin"));

    actionCollection->setComponentDisplayName(i18n("KWin"));
    actionCollection->setConfigGroup(QStringLiteral("windowview"));
    actionCollection->setConfigGlobal(true);

    const QKeySequence defaultToggleShortcut = Qt::CTRL | Qt::Key_F9;
    QAction* toggleAction = actionCollection->addAction(QStringLiteral("Expose"));
    toggleAction->setText(i18n("Toggle Present Windows (Current desktop)"));
    toggleAction->setProperty("isConfigurationAction", true);
    KGlobalAccel::self()->setDefaultShortcut(toggleAction, {defaultToggleShortcut});
    KGlobalAccel::self()->setShortcut(toggleAction, {defaultToggleShortcut});

    const QKeySequence defaultToggleShortcutAll = Qt::CTRL | Qt::Key_F10;
    toggleAction = actionCollection->addAction(QStringLiteral("ExposeAll"));
    toggleAction->setText(i18n("Toggle Present Windows (All desktops)"));
    toggleAction->setProperty("isConfigurationAction", true);
    KGlobalAccel::self()->setDefaultShortcut(toggleAction, {defaultToggleShortcutAll});
    KGlobalAccel::self()->setShortcut(toggleAction, {defaultToggleShortcutAll});

    const QKeySequence defaultToggleShortcutClass = Qt::CTRL | Qt::Key_F7;
    toggleAction = actionCollection->addAction(QStringLiteral("ExposeClass"));
    toggleAction->setText(i18n("Toggle Present Windows (Window class)"));
    toggleAction->setProperty("isConfigurationAction", true);
    KGlobalAccel::self()->setDefaultShortcut(toggleAction, {defaultToggleShortcutClass});
    KGlobalAccel::self()->setShortcut(toggleAction, {defaultToggleShortcutClass});

    toggleAction = actionCollection->addAction(QStringLiteral("ExposeClassCurrentDesktop"));
    toggleAction->setText(i18n("Toggle Present Windows (Window class on current desktop)"));
    toggleAction->setProperty("isConfigurationAction", true);
    KGlobalAccel::self()->setDefaultShortcut(toggleAction,
                                             QList<QKeySequence>()); // no default shortcut
    KGlobalAccel::self()->setShortcut(toggleAction, QList<QKeySequence>());

    ui.shortcutsEditor->addCollection(actionCollection);
    connect(ui.shortcutsEditor, &KShortcutsEditor::keyChange, this, &KCModule::markAsChanged);
}

void WindowViewEffectConfig::save()
{
    KCModule::save();
    ui.shortcutsEditor->save();

    OrgKdeKwinEffectsInterface interface(
        QStringLiteral("org.kde.KWin"), QStringLiteral("/Effects"), QDBusConnection::sessionBus());
    interface.reconfigureEffect(QStringLiteral("windowview"));
}

void WindowViewEffectConfig::defaults()
{
    ui.shortcutsEditor->allDefault();
    KCModule::defaults();
}

}

#include "windowvieweffectkcm.moc"
