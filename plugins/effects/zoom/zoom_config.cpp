/*
SPDX-FileCopyrightText: 2007 Rivo Laks <rivolaks@hot.ee>
SPDX-FileCopyrightText: 2010 Sebastian Sauer <sebsauer@kdab.com>

SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "zoom_config.h"
// KConfigSkeleton
#include "zoomconfig.h"
#include <config-como.h>
#include <kwineffects_interface.h>

#include <QAction>

#include <KActionCollection>
#include <KGlobalAccel>
#include <KLocalizedString>
#include <KPluginFactory>

#include <QVBoxLayout>

K_PLUGIN_CLASS(KWin::ZoomEffectConfig)

namespace KWin
{

ZoomEffectConfig::ZoomEffectConfig(QObject* parent, const KPluginMetaData& data)
    : KCModule(parent, data)
{
    ZoomConfig::instance(KWIN_CONFIG);
    m_ui.setupUi(widget());

    addConfig(ZoomConfig::self(), widget());

    connect(m_ui.editor, &KShortcutsEditor::keyChange, this, &KCModule::markAsChanged);

#if !HAVE_ACCESSIBILITY
    m_ui->kcfg_EnableFocusTracking->setVisible(false);
    m_ui->kcfg_EnableTextCaretTracking->setVisible(false);
#endif

    // Shortcut config. The shortcut belongs to the component "kwin"!
    KActionCollection* actionCollection = new KActionCollection(this, QStringLiteral("kwin"));
    actionCollection->setComponentDisplayName(i18n("KWin"));
    actionCollection->setConfigGroup(QStringLiteral("Zoom"));
    actionCollection->setConfigGlobal(true);

    QAction* a;
    a = actionCollection->addAction(KStandardAction::ZoomIn);
    a->setProperty("isConfigurationAction", true);
    KGlobalAccel::self()->setDefaultShortcut(a, QList<QKeySequence>() << (Qt::META | Qt::Key_Plus));
    KGlobalAccel::self()->setShortcut(
        a, QList<QKeySequence>() << (Qt::META | Qt::Key_Plus) << (Qt::META | Qt::Key_Equal));

    a = actionCollection->addAction(KStandardAction::ZoomOut);
    a->setProperty("isConfigurationAction", true);
    KGlobalAccel::self()->setDefaultShortcut(
        a, QList<QKeySequence>() << static_cast<Qt::Key>(Qt::META) + Qt::Key_Minus);
    KGlobalAccel::self()->setShortcut(
        a, QList<QKeySequence>() << static_cast<Qt::Key>(Qt::META) + Qt::Key_Minus);

    a = actionCollection->addAction(KStandardAction::ActualSize);
    a->setProperty("isConfigurationAction", true);
    KGlobalAccel::self()->setDefaultShortcut(
        a, QList<QKeySequence>() << static_cast<Qt::Key>(Qt::META) + Qt::Key_0);
    KGlobalAccel::self()->setShortcut(
        a, QList<QKeySequence>() << static_cast<Qt::Key>(Qt::META) + Qt::Key_0);

    a = actionCollection->addAction(QStringLiteral("MoveZoomLeft"));
    a->setIcon(QIcon::fromTheme(QStringLiteral("go-previous")));
    a->setText(i18n("Move Left"));
    a->setProperty("isConfigurationAction", true);
    KGlobalAccel::self()->setDefaultShortcut(
        a, QList<QKeySequence>() << (Qt::META | Qt::CTRL | Qt::Key_Left));
    KGlobalAccel::self()->setShortcut(
        a, QList<QKeySequence>() << (Qt::META | Qt::CTRL | Qt::Key_Left));

    a = actionCollection->addAction(QStringLiteral("MoveZoomRight"));
    a->setIcon(QIcon::fromTheme(QStringLiteral("go-next")));
    a->setText(i18n("Move Right"));
    a->setProperty("isConfigurationAction", true);
    KGlobalAccel::self()->setDefaultShortcut(
        a, QList<QKeySequence>() << (Qt::META | Qt::CTRL | Qt::Key_Right));
    KGlobalAccel::self()->setShortcut(
        a, QList<QKeySequence>() << (Qt::META | Qt::CTRL | Qt::Key_Right));

    a = actionCollection->addAction(QStringLiteral("MoveZoomUp"));
    a->setIcon(QIcon::fromTheme(QStringLiteral("go-up")));
    a->setText(i18n("Move Up"));
    a->setProperty("isConfigurationAction", true);
    KGlobalAccel::self()->setDefaultShortcut(
        a, QList<QKeySequence>() << (Qt::META | Qt::CTRL | Qt::Key_Up));
    KGlobalAccel::self()->setShortcut(a,
                                      QList<QKeySequence>() << (Qt::META | Qt::CTRL | Qt::Key_Up));

    a = actionCollection->addAction(QStringLiteral("MoveZoomDown"));
    a->setIcon(QIcon::fromTheme(QStringLiteral("go-down")));
    a->setText(i18n("Move Down"));
    a->setProperty("isConfigurationAction", true);
    KGlobalAccel::self()->setDefaultShortcut(
        a, QList<QKeySequence>() << (Qt::META | Qt::CTRL | Qt::Key_Down));
    KGlobalAccel::self()->setShortcut(
        a, QList<QKeySequence>() << (Qt::META | Qt::CTRL | Qt::Key_Down));

    a = actionCollection->addAction(QStringLiteral("MoveMouseToFocus"));
    a->setIcon(QIcon::fromTheme(QStringLiteral("view-restore")));
    a->setText(i18n("Move Mouse to Focus"));
    a->setProperty("isConfigurationAction", true);
    KGlobalAccel::self()->setDefaultShortcut(
        a, QList<QKeySequence>() << static_cast<Qt::Key>(Qt::META) + Qt::Key_F5);
    KGlobalAccel::self()->setShortcut(
        a, QList<QKeySequence>() << static_cast<Qt::Key>(Qt::META) + Qt::Key_F5);

    a = actionCollection->addAction(QStringLiteral("MoveMouseToCenter"));
    a->setIcon(QIcon::fromTheme(QStringLiteral("view-restore")));
    a->setText(i18n("Move Mouse to Center"));
    a->setProperty("isConfigurationAction", true);
    KGlobalAccel::self()->setDefaultShortcut(
        a, QList<QKeySequence>() << static_cast<Qt::Key>(Qt::META) + Qt::Key_F6);
    KGlobalAccel::self()->setShortcut(
        a, QList<QKeySequence>() << static_cast<Qt::Key>(Qt::META) + Qt::Key_F6);

    m_ui.editor->addCollection(actionCollection);
}

void ZoomEffectConfig::save()
{
    m_ui.editor->save(); // undo() will restore to this state from now on
    KCModule::save();
    OrgKdeKwinEffectsInterface interface(
        QStringLiteral("org.kde.KWin"), QStringLiteral("/Effects"), QDBusConnection::sessionBus());
    interface.reconfigureEffect(QStringLiteral("zoom"));
}

} // namespace

#include "zoom_config.moc"
