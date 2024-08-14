/*
SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>

SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "bridge_qobject.h"

#include <como/win/space_qobject.h>

#include <KConfigGroup>
#include <KDecoration2/Private/DecorationSettingsPrivate>
#include <QFontDatabase>
#include <QObject>

namespace como::win::deco
{

template<typename Space>
class settings : public QObject, public KDecoration2::DecorationSettingsPrivate
{
public:
    settings(Space& space, KDecoration2::DecorationSettings* parent)
        : QObject()
        , DecorationSettingsPrivate(parent)
        , m_borderSize(KDecoration2::BorderSize::Normal)
        , space{space}
    {
        initButtons();
        readSettings();

        auto comp_qobject = space.base.mod.render->qobject.get();
        using comp_qobject_t = std::remove_pointer_t<decltype(comp_qobject)>;

        auto c = connect(comp_qobject,
                         &comp_qobject_t::compositingToggled,
                         parent,
                         &KDecoration2::DecorationSettings::alphaChannelSupportedChanged);
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

    KDecoration2::BorderSize borderSize() const override
    {
        return m_borderSize;
    }

    QVector<KDecoration2::DecorationButtonType> decorationButtonsLeft() const override
    {
        return m_leftButtons;
    }

    QVector<KDecoration2::DecorationButtonType> decorationButtonsRight() const override
    {
        return m_rightButtons;
    }

    QFont font() const override
    {
        return m_font;
    }

private:
    QHash<KDecoration2::DecorationButtonType, QChar> s_buttonNames;
    void initButtons()
    {
        if (!s_buttonNames.isEmpty()) {
            return;
        }
        s_buttonNames[KDecoration2::DecorationButtonType::Menu] = QChar('M');
        s_buttonNames[KDecoration2::DecorationButtonType::ApplicationMenu] = QChar('N');
        s_buttonNames[KDecoration2::DecorationButtonType::OnAllDesktops] = QChar('S');
        s_buttonNames[KDecoration2::DecorationButtonType::ContextHelp] = QChar('H');
        s_buttonNames[KDecoration2::DecorationButtonType::Minimize] = QChar('I');
        s_buttonNames[KDecoration2::DecorationButtonType::Maximize] = QChar('A');
        s_buttonNames[KDecoration2::DecorationButtonType::Close] = QChar('X');
        s_buttonNames[KDecoration2::DecorationButtonType::KeepAbove] = QChar('F');
        s_buttonNames[KDecoration2::DecorationButtonType::KeepBelow] = QChar('B');
        s_buttonNames[KDecoration2::DecorationButtonType::Shade] = QChar('L');
        s_buttonNames[KDecoration2::DecorationButtonType::Spacer] = QChar('_');
    }

    QString buttonsToString(QVector<KDecoration2::DecorationButtonType> const& buttons) const
    {
        auto buttonToString = [this](KDecoration2::DecorationButtonType button) -> QChar {
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

    KDecoration2::BorderSize stringToSize(QString const& name)
    {
        static const QMap<QString, KDecoration2::BorderSize> s_sizes
            = QMap<QString, KDecoration2::BorderSize>(
                {{QStringLiteral("None"), KDecoration2::BorderSize::None},
                 {QStringLiteral("NoSides"), KDecoration2::BorderSize::NoSides},
                 {QStringLiteral("Tiny"), KDecoration2::BorderSize::Tiny},
                 {QStringLiteral("Normal"), KDecoration2::BorderSize::Normal},
                 {QStringLiteral("Large"), KDecoration2::BorderSize::Large},
                 {QStringLiteral("VeryLarge"), KDecoration2::BorderSize::VeryLarge},
                 {QStringLiteral("Huge"), KDecoration2::BorderSize::Huge},
                 {QStringLiteral("VeryHuge"), KDecoration2::BorderSize::VeryHuge},
                 {QStringLiteral("Oversized"), KDecoration2::BorderSize::Oversized}});
        auto it = s_sizes.constFind(name);
        if (it == s_sizes.constEnd()) {
            // non sense values are interpreted just like normal
            return KDecoration2::BorderSize::Normal;
        }
        return it.value();
    }

    void readSettings()
    {
        KConfigGroup config = space.base.config.main->group(QStringLiteral("org.kde.kdecoration2"));
        const auto& left
            = readDecorationButtons(config,
                                    "ButtonsOnLeft",
                                    QVector<KDecoration2::DecorationButtonType>(
                                        {KDecoration2::DecorationButtonType::Menu,
                                         KDecoration2::DecorationButtonType::OnAllDesktops}));
        if (left != m_leftButtons) {
            m_leftButtons = left;
            Q_EMIT decorationSettings() -> decorationButtonsLeftChanged(m_leftButtons);
        }
        const auto& right
            = readDecorationButtons(config,
                                    "ButtonsOnRight",
                                    QVector<KDecoration2::DecorationButtonType>(
                                        {KDecoration2::DecorationButtonType::ContextHelp,
                                         KDecoration2::DecorationButtonType::Minimize,
                                         KDecoration2::DecorationButtonType::Maximize,
                                         KDecoration2::DecorationButtonType::Close}));
        if (right != m_rightButtons) {
            m_rightButtons = right;
            Q_EMIT decorationSettings() -> decorationButtonsRightChanged(m_rightButtons);
        }
        space.appmenu->setViewEnabled(
            left.contains(KDecoration2::DecorationButtonType::ApplicationMenu)
            || right.contains(KDecoration2::DecorationButtonType::ApplicationMenu));
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

    QVector<KDecoration2::DecorationButtonType>
    readDecorationButtons(KConfigGroup const& config,
                          char const* key,
                          QVector<KDecoration2::DecorationButtonType> const& defaultValue) const
    {
        auto buttonsFromString = [this](auto const& buttons) {
            QVector<KDecoration2::DecorationButtonType> ret;
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

    QVector<KDecoration2::DecorationButtonType> m_leftButtons;
    QVector<KDecoration2::DecorationButtonType> m_rightButtons;
    KDecoration2::BorderSize m_borderSize;
    bool m_autoBorderSize = true;
    bool m_closeDoubleClickMenu = false;
    QFont m_font;
    Space& space;
};

}
