/*
SPDX-FileCopyrightText: 2012 Martin Gräßlin <mgraesslin@kde.org>

SPDX-License-Identifier: GPL-2.0-or-later

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#include "decorationoptions.h"
#include <KConfigGroup>
#include <KDecoration3/DecoratedWindow>
#include <KDecoration3/DecorationSettings>
#include <KSharedConfig>
#include <QGuiApplication>
#include <QStyleHints>

namespace como
{

ColorSettings::ColorSettings(const QPalette& pal)
{
    init(pal);
}

void ColorSettings::update(const QPalette& pal)
{
    init(pal);
}

void ColorSettings::init(const QPalette& pal)
{
    m_palette = pal;
    KConfigGroup wmConfig(KSharedConfig::openConfig(QStringLiteral("kdeglobals")),
                          QStringLiteral("WM"));
    m_activeFrameColor = wmConfig.readEntry("frame", pal.color(QPalette::Active, QPalette::Window));
    m_inactiveFrameColor = wmConfig.readEntry("inactiveFrame", m_activeFrameColor);
    m_activeTitleBarColor
        = wmConfig.readEntry("activeBackground", pal.color(QPalette::Active, QPalette::Highlight));
    m_inactiveTitleBarColor = wmConfig.readEntry("inactiveBackground", m_inactiveFrameColor);
    m_activeTitleBarBlendColor
        = wmConfig.readEntry("activeBlend", m_activeTitleBarColor.darker(110));
    m_inactiveTitleBarBlendColor
        = wmConfig.readEntry("inactiveBlend", m_inactiveTitleBarColor.darker(110));
    m_activeFontColor = wmConfig.readEntry("activeForeground",
                                           pal.color(QPalette::Active, QPalette::HighlightedText));
    m_inactiveFontColor = wmConfig.readEntry("inactiveForeground", m_activeFontColor.darker());
    m_activeButtonColor = wmConfig.readEntry("activeTitleBtnBg", m_activeFrameColor.lighter(130));
    m_inactiveButtonColor
        = wmConfig.readEntry("inactiveTitleBtnBg", m_inactiveFrameColor.lighter(130));
    m_activeHandle = wmConfig.readEntry("handle", m_activeFrameColor);
    m_inactiveHandle = wmConfig.readEntry("inactiveHandle", m_activeHandle);
}

DecorationOptions::DecorationOptions(QObject* parent)
    : QObject(parent)
    , m_active(true)
    , m_decoration(nullptr)
    , m_colors(ColorSettings(QPalette()))
{
    connect(
        this, &DecorationOptions::decorationChanged, this, &DecorationOptions::slotActiveChanged);
    connect(this, &DecorationOptions::decorationChanged, this, &DecorationOptions::colorsChanged);
    connect(this, &DecorationOptions::decorationChanged, this, &DecorationOptions::fontChanged);
    connect(
        this, &DecorationOptions::decorationChanged, this, &DecorationOptions::titleButtonsChanged);
}

DecorationOptions::~DecorationOptions()
{
}

QColor DecorationOptions::borderColor() const
{
    return m_active ? m_colors.activeFrame() : m_colors.inactiveFrame();
}

QColor DecorationOptions::buttonColor() const
{
    return m_active ? m_colors.activeButtonColor() : m_colors.inactiveButtonColor();
}

QColor DecorationOptions::fontColor() const
{
    return m_active ? m_colors.activeFont() : m_colors.inactiveFont();
}

QColor DecorationOptions::resizeHandleColor() const
{
    return m_active ? m_colors.activeHandle() : m_colors.inactiveHandle();
}

QColor DecorationOptions::titleBarBlendColor() const
{
    return m_active ? m_colors.activeTitleBarBlendColor() : m_colors.inactiveTitleBarBlendColor();
}

QColor DecorationOptions::titleBarColor() const
{
    return m_active ? m_colors.activeTitleBarColor() : m_colors.inactiveTitleBarColor();
}

QFont DecorationOptions::titleFont() const
{
    return m_decoration ? m_decoration->settings()->font() : QFont();
}

static int decorationButton(KDecoration3::DecorationButtonType type)
{
    switch (type) {
    case KDecoration3::DecorationButtonType::Menu:
        return DecorationOptions::DecorationButtonMenu;
    case KDecoration3::DecorationButtonType::ApplicationMenu:
        return DecorationOptions::DecorationButtonApplicationMenu;
    case KDecoration3::DecorationButtonType::OnAllDesktops:
        return DecorationOptions::DecorationButtonOnAllDesktops;
    case KDecoration3::DecorationButtonType::Minimize:
        return DecorationOptions::DecorationButtonMinimize;
    case KDecoration3::DecorationButtonType::Maximize:
        return DecorationOptions::DecorationButtonMaximizeRestore;
    case KDecoration3::DecorationButtonType::Close:
        return DecorationOptions::DecorationButtonClose;
    case KDecoration3::DecorationButtonType::ContextHelp:
        return DecorationOptions::DecorationButtonQuickHelp;
    case KDecoration3::DecorationButtonType::Shade:
        return DecorationOptions::DecorationButtonShade;
    case KDecoration3::DecorationButtonType::KeepBelow:
        return DecorationOptions::DecorationButtonKeepBelow;
    case KDecoration3::DecorationButtonType::KeepAbove:
        return DecorationOptions::DecorationButtonKeepAbove;
    default:
        return DecorationOptions::DecorationButtonNone;
    }
}

QList<int> DecorationOptions::titleButtonsLeft() const
{
    QList<int> ret;
    if (m_decoration) {
        auto const buttons_left = m_decoration->settings()->decorationButtonsLeft();
        for (auto it : buttons_left) {
            ret << decorationButton(it);
        }
    }
    return ret;
}

QList<int> DecorationOptions::titleButtonsRight() const
{
    QList<int> ret;
    if (m_decoration) {
        auto const buttons_right = m_decoration->settings()->decorationButtonsRight();
        for (auto it : buttons_right) {
            ret << decorationButton(it);
        }
    }
    return ret;
}

KDecoration3::Decoration* DecorationOptions::decoration() const
{
    return m_decoration;
}

void DecorationOptions::setDecoration(KDecoration3::Decoration* decoration)
{
    if (m_decoration == decoration) {
        return;
    }
    if (m_decoration) {
        // disconnect from existing decoration
        disconnect(m_decoration->window(),
                   &KDecoration3::DecoratedWindow::activeChanged,
                   this,
                   &DecorationOptions::slotActiveChanged);
        auto s = m_decoration->settings();
        disconnect(s.get(),
                   &KDecoration3::DecorationSettings::fontChanged,
                   this,
                   &DecorationOptions::fontChanged);
        disconnect(s.get(),
                   &KDecoration3::DecorationSettings::decorationButtonsLeftChanged,
                   this,
                   &DecorationOptions::titleButtonsChanged);
        disconnect(s.get(),
                   &KDecoration3::DecorationSettings::decorationButtonsRightChanged,
                   this,
                   &DecorationOptions::titleButtonsChanged);
        disconnect(m_paletteConnection);
    }
    m_decoration = decoration;
    connect(m_decoration->window(),
            &KDecoration3::DecoratedWindow::activeChanged,
            this,
            &DecorationOptions::slotActiveChanged);
    m_paletteConnection = connect(m_decoration->window(),
                                  &KDecoration3::DecoratedWindow::paletteChanged,
                                  this,
                                  [this](const QPalette& pal) {
                                      m_colors.update(pal);
                                      Q_EMIT colorsChanged();
                                  });
    auto s = m_decoration->settings();
    connect(s.get(),
            &KDecoration3::DecorationSettings::fontChanged,
            this,
            &DecorationOptions::fontChanged);
    connect(s.get(),
            &KDecoration3::DecorationSettings::decorationButtonsLeftChanged,
            this,
            &DecorationOptions::titleButtonsChanged);
    connect(s.get(),
            &KDecoration3::DecorationSettings::decorationButtonsRightChanged,
            this,
            &DecorationOptions::titleButtonsChanged);
    Q_EMIT decorationChanged();
}

void DecorationOptions::slotActiveChanged()
{
    if (!m_decoration) {
        return;
    }
    if (m_active == m_decoration->window()->isActive()) {
        return;
    }
    m_active = m_decoration->window()->isActive();
    Q_EMIT colorsChanged();
    Q_EMIT fontChanged();
}

int DecorationOptions::mousePressAndHoldInterval() const
{
    return QGuiApplication::styleHints()->mousePressAndHoldInterval();
}

Borders::Borders(QObject* parent)
    : QObject(parent)
    , m_left(0)
    , m_right(0)
    , m_top(0)
    , m_bottom(0)
{
}

Borders::~Borders()
{
}

void Borders::setLeft(int left)
{
    if (m_left != left) {
        m_left = left;
        Q_EMIT leftChanged();
    }
}
void Borders::setRight(int right)
{
    if (m_right != right) {
        m_right = right;
        Q_EMIT rightChanged();
    }
}
void Borders::setTop(int top)
{
    if (m_top != top) {
        m_top = top;
        Q_EMIT topChanged();
    }
}
void Borders::setBottom(int bottom)
{
    if (m_bottom != bottom) {
        m_bottom = bottom;
        Q_EMIT bottomChanged();
    }
}

void Borders::setAllBorders(int border)
{
    setBorders(border);
    setTitle(border);
}

void Borders::setBorders(int border)
{
    setSideBorders(border);
    setBottom(border);
}

void Borders::setSideBorders(int border)
{
    setLeft(border);
    setRight(border);
}

void Borders::setTitle(int value)
{
    setTop(value);
}

Borders::operator QMarginsF() const
{
    return QMarginsF(m_left, m_top, m_right, m_bottom);
}

} // namespace
