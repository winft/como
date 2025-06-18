/*
SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>

SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "bridge_qobject.h"

#include <como/win/space_qobject.h>

#include <KConfigGroup>
#include <KDecoration3/Private/DecorationSettingsPrivate>
#include <QFontDatabase>
#include <QObject>

namespace como::win::deco
{

template<typename Space>
class settings : public QObject, public KDecoration3::DecorationSettingsPrivate
{
public:
    settings(Space& space, KDecoration3::DecorationSettings* parent)
        : QObject()
        , DecorationSettingsPrivate(parent)
        , m_borderSize(KDecoration3::BorderSize::Normal)
        , space{space}
    {
        initButtons();
        readSettings();

        auto comp_qobject = space.base.mod.render->qobject.get();
        using comp_qobject_t = std::remove_pointer_t<decltype(comp_qobject)>;

        auto c = connect(comp_qobject,
                         &comp_qobject_t::compositingToggled,
                         parent,
                         &KDecoration3::DecorationSettings::alphaChannelSupportedChanged);
        connect(space.subspace_manager->qobject.get(),
                &decltype(space.subspace_manager->qobject)::element_type::countChanged,
                this,
                [parent](uint previous, uint current) {
                    if (previous != 1 && current != 1) {
                        return;
                    }
                    Q_EMIT parent->onAllDesktopsAvailableChanged(current > 1);
                });
        // prevent changes in Decoration due to compositor being destroyed
        connect(comp_qobject, &comp_qobject_t::aboutToDestroy, this, [c] { disconnect(c); });
        connect(
            space.qobject.get(), &win::space_qobject::configChanged, this, &settings::readSettings);
        connect(space.deco->qobject.get(),
                &bridge_qobject::metaDataLoaded,
                this,
                &settings::readSettings);
    }

    bool isAlphaChannelSupported() const override
    {
        auto comp = space.base.mod.render.get();
        using comp_t = std::remove_pointer_t<decltype(comp)>;
        return comp->state == comp_t::state_t::on;
    }

    bool isOnAllDesktopsAvailable() const override
    {
        return space.subspace_manager->subspaces.size() > 1;
    }

    bool isCloseOnDoubleClickOnMenu() const override
    {
        return m_closeDoubleClickMenu;
    }

    KDecoration3::BorderSize borderSize() const override
    {
        return m_borderSize;
    }

    QVector<KDecoration3::DecorationButtonType> decorationButtonsLeft() const override
    {
        return m_leftButtons;
    }

    QVector<KDecoration3::DecorationButtonType> decorationButtonsRight() const override
    {
        return m_rightButtons;
    }

    QFont font() const override
    {
        return m_font;
    }

private:
    QHash<KDecoration3::DecorationButtonType, QChar> s_buttonNames;
    void initButtons()
    {
        if (!s_buttonNames.isEmpty()) {
            return;
        }
        s_buttonNames[KDecoration3::DecorationButtonType::Menu] = QChar('M');
        s_buttonNames[KDecoration3::DecorationButtonType::ApplicationMenu] = QChar('N');
        s_buttonNames[KDecoration3::DecorationButtonType::OnAllDesktops] = QChar('S');
        s_buttonNames[KDecoration3::DecorationButtonType::ContextHelp] = QChar('H');
        s_buttonNames[KDecoration3::DecorationButtonType::Minimize] = QChar('I');
        s_buttonNames[KDecoration3::DecorationButtonType::Maximize] = QChar('A');
        s_buttonNames[KDecoration3::DecorationButtonType::Close] = QChar('X');
        s_buttonNames[KDecoration3::DecorationButtonType::KeepAbove] = QChar('F');
        s_buttonNames[KDecoration3::DecorationButtonType::KeepBelow] = QChar('B');
        s_buttonNames[KDecoration3::DecorationButtonType::Shade] = QChar('L');
        s_buttonNames[KDecoration3::DecorationButtonType::Spacer] = QChar('_');
    }

    QString buttonsToString(QVector<KDecoration3::DecorationButtonType> const& buttons) const
    {
        auto buttonToString = [this](KDecoration3::DecorationButtonType button) -> QChar {
            const auto it = s_buttonNames.constFind(button);
            if (it != s_buttonNames.constEnd()) {
                return it.value();
            }
            return QChar();
        };
        QString ret;
        for (auto button : buttons) {
            ret.append(buttonToString(button));
        }
        return ret;
    }

    KDecoration3::BorderSize stringToSize(QString const& name)
    {
        static const QMap<QString, KDecoration3::BorderSize> s_sizes
            = QMap<QString, KDecoration3::BorderSize>(
                {{QStringLiteral("None"), KDecoration3::BorderSize::None},
                 {QStringLiteral("NoSides"), KDecoration3::BorderSize::NoSides},
                 {QStringLiteral("Tiny"), KDecoration3::BorderSize::Tiny},
                 {QStringLiteral("Normal"), KDecoration3::BorderSize::Normal},
                 {QStringLiteral("Large"), KDecoration3::BorderSize::Large},
                 {QStringLiteral("VeryLarge"), KDecoration3::BorderSize::VeryLarge},
                 {QStringLiteral("Huge"), KDecoration3::BorderSize::Huge},
                 {QStringLiteral("VeryHuge"), KDecoration3::BorderSize::VeryHuge},
                 {QStringLiteral("Oversized"), KDecoration3::BorderSize::Oversized}});
        auto it = s_sizes.constFind(name);
        if (it == s_sizes.constEnd()) {
            // non sense values are interpreted just like normal
            return KDecoration3::BorderSize::Normal;
        }
        return it.value();
    }

    void readSettings()
    {
        KConfigGroup config = space.base.config.main->group(QStringLiteral("org.kde.kdecoration3"));
        const auto& left
            = readDecorationButtons(config,
                                    "ButtonsOnLeft",
                                    QVector<KDecoration3::DecorationButtonType>(
                                        {KDecoration3::DecorationButtonType::Menu,
                                         KDecoration3::DecorationButtonType::OnAllDesktops}));
        if (left != m_leftButtons) {
            m_leftButtons = left;
            Q_EMIT decorationSettings() -> decorationButtonsLeftChanged(m_leftButtons);
        }
        const auto& right
            = readDecorationButtons(config,
                                    "ButtonsOnRight",
                                    QVector<KDecoration3::DecorationButtonType>(
                                        {KDecoration3::DecorationButtonType::ContextHelp,
                                         KDecoration3::DecorationButtonType::Minimize,
                                         KDecoration3::DecorationButtonType::Maximize,
                                         KDecoration3::DecorationButtonType::Close}));
        if (right != m_rightButtons) {
            m_rightButtons = right;
            Q_EMIT decorationSettings() -> decorationButtonsRightChanged(m_rightButtons);
        }
        space.appmenu->setViewEnabled(
            left.contains(KDecoration3::DecorationButtonType::ApplicationMenu)
            || right.contains(KDecoration3::DecorationButtonType::ApplicationMenu));
        const bool close = config.readEntry("CloseOnDoubleClickOnMenu", false);
        if (close != m_closeDoubleClickMenu) {
            m_closeDoubleClickMenu = close;
            Q_EMIT decorationSettings() -> closeOnDoubleClickOnMenuChanged(m_closeDoubleClickMenu);
        }
        m_autoBorderSize = config.readEntry("BorderSizeAuto", true);

        auto size = stringToSize(config.readEntry("BorderSize", QStringLiteral("Normal")));
        if (m_autoBorderSize) {
            /* Falls back to Normal border size, if the plugin does not provide a valid
             * recommendation.
             */
            size = stringToSize(space.deco->recommendedBorderSize());
        }
        if (size != m_borderSize) {
            m_borderSize = size;
            Q_EMIT decorationSettings() -> borderSizeChanged(m_borderSize);
        }
        const QFont font = QFontDatabase::systemFont(QFontDatabase::TitleFont);
        if (font != m_font) {
            m_font = font;
            Q_EMIT decorationSettings() -> fontChanged(m_font);
        }

        Q_EMIT decorationSettings() -> reconfigured();
    }

    QVector<KDecoration3::DecorationButtonType>
    readDecorationButtons(KConfigGroup const& config,
                          char const* key,
                          QVector<KDecoration3::DecorationButtonType> const& defaultValue) const
    {
        auto buttonsFromString = [this](auto const& buttons) {
            QVector<KDecoration3::DecorationButtonType> ret;
            for (auto it = buttons.begin(); it != buttons.end(); ++it) {
                for (auto it2 = s_buttonNames.constBegin(); it2 != s_buttonNames.constEnd();
                     ++it2) {
                    if (it2.value() == (*it)) {
                        ret << it2.key();
                    }
                }
            }
            return ret;
        };
        return buttonsFromString(config.readEntry(key, buttonsToString(defaultValue)));
    }

    QVector<KDecoration3::DecorationButtonType> m_leftButtons;
    QVector<KDecoration3::DecorationButtonType> m_rightButtons;
    KDecoration3::BorderSize m_borderSize;
    bool m_autoBorderSize = true;
    bool m_closeDoubleClickMenu = false;
    QFont m_font;
    Space& space;
};

}
