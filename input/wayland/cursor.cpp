/*
    SPDX-FileCopyrightText: 2013 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2021 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "cursor.h"

#include "cursor_image.h"
#include "redirect.h"

namespace KWin::input::wayland
{

cursor::cursor(wayland::redirect* redirect)
    : input::cursor()
    , cursor_image{std::make_unique<wayland::cursor_image>()}
    , redirect{redirect}
{
    QObject::connect(redirect, &redirect::globalPointerChanged, this, &cursor::slot_pos_changed);
    QObject::connect(
        redirect, &redirect::pointerButtonStateChanged, this, &cursor::slot_pointer_button_changed);
    QObject::connect(
        redirect, &redirect::keyboardModifiersChanged, this, &cursor::slot_modifiers_changed);
}

cursor::~cursor() = default;

QImage cursor::image() const
{
    return cursor_image->image();
}

QPoint cursor::hotspot() const
{
    return cursor_image->hotSpot();
}

void cursor::mark_as_rendered()
{
    cursor_image->markAsRendered();
}

PlatformCursorImage cursor::platform_image() const
{
    return PlatformCursorImage(image(), hotspot());
}

void cursor::do_set_pos()
{
    if (redirect->supportsPointerWarping()) {
        redirect->warpPointer(current_pos());
    }
    slot_pos_changed(redirect->globalPointer());
    Q_EMIT pos_changed(current_pos());
}

void cursor::slot_pos_changed(const QPointF& pos)
{
    auto const oldPos = current_pos();
    update_pos(pos.toPoint());
    Q_EMIT mouse_changed(pos.toPoint(),
                         oldPos,
                         m_currentButtons,
                         m_currentButtons,
                         redirect->keyboardModifiers(),
                         redirect->keyboardModifiers());
}

void cursor::slot_modifiers_changed(Qt::KeyboardModifiers mods, Qt::KeyboardModifiers oldMods)
{
    Q_EMIT mouse_changed(
        current_pos(), current_pos(), m_currentButtons, m_currentButtons, mods, oldMods);
}

void cursor::slot_pointer_button_changed()
{
    Qt::MouseButtons const oldButtons = m_currentButtons;
    m_currentButtons = redirect->qtButtonStates();
    auto const pos = current_pos();
    Q_EMIT mouse_changed(pos,
                         pos,
                         m_currentButtons,
                         oldButtons,
                         redirect->keyboardModifiers(),
                         redirect->keyboardModifiers());
}

void cursor::do_start_image_tracking()
{
    QObject::connect(cursor_image.get(), &cursor_image::changed, this, &cursor::image_changed);
}

void cursor::do_stop_image_tracking()
{
    QObject::disconnect(cursor_image.get(), &cursor_image::changed, this, &cursor::image_changed);
}

}
