/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "expoarea.h"
#include <como/render/effect/interface/effects_handler.h>

namespace como
{

ExpoArea::ExpoArea(QObject* parent)
    : QObject(parent)
{
}

int ExpoArea::x() const
{
    return m_rect.x();
}

int ExpoArea::y() const
{
    return m_rect.y();
}

int ExpoArea::width() const
{
    return m_rect.width();
}

int ExpoArea::height() const
{
    return m_rect.height();
}

EffectScreen* ExpoArea::screen() const
{
    return m_screen;
}

void ExpoArea::setScreen(EffectScreen* screen)
{
    if (m_screen != screen) {
        if (m_screen) {
            disconnect(m_screen, &EffectScreen::geometryChanged, this, &ExpoArea::update);
        }
        m_screen = screen;
        if (m_screen) {
            connect(m_screen, &EffectScreen::geometryChanged, this, &ExpoArea::update);
        }
        update();
        Q_EMIT screenChanged();
    }
}

void ExpoArea::update()
{
    if (!m_screen) {
        return;
    }
    const QRect oldRect = m_rect;

    m_rect = effects->clientArea(MaximizeArea, m_screen, effects->currentDesktop());

    // Map the area to the output local coordinates.
    m_rect.translate(-m_screen->geometry().topLeft());

    if (oldRect.x() != m_rect.x()) {
        Q_EMIT xChanged();
    }
    if (oldRect.y() != m_rect.y()) {
        Q_EMIT yChanged();
    }
    if (oldRect.width() != m_rect.width()) {
        Q_EMIT widthChanged();
    }
    if (oldRect.height() != m_rect.height()) {
        Q_EMIT heightChanged();
    }
}

}
