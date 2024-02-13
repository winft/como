/*
    SPDX-FileCopyrightText: 2013 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2021 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "cursor_image.h"

#include <como/input/cursor.h>
#include <como/input/redirect_qobject.h>
#include <como/input/xkb/helpers.h>

#include <QPointF>
#include <Wrapland/Server/seat.h>
#include <memory>
#include <utility>

namespace como::input::wayland
{

template<typename Redirect>
class cursor : public input::cursor
{
public:
    using type = cursor<Redirect>;
    using redirect_t = Redirect;

    cursor(Redirect& redirect)
        : input::cursor(redirect.platform.config.main)
        , redirect{redirect}
        , cursor_image{std::make_unique<wayland::cursor_image<type>>(*this)}
    {
        QObject::connect(redirect.qobject.get(),
                         &redirect_qobject::globalPointerChanged,
                         this,
                         &type::slot_pos_changed);
        QObject::connect(redirect.qobject.get(),
                         &redirect_qobject::pointerButtonStateChanged,
                         this,
                         &type::slot_pointer_button_changed);
        QObject::connect(redirect.qobject.get(),
                         &redirect_qobject::keyboardModifiersChanged,
                         this,
                         &type::slot_modifiers_changed);

        if constexpr (requires(Redirect redirect) { redirect.space.xcb_cursors; }) {
            QObject::connect(this, &cursor::theme_changed, redirect.space.qobject.get(), [this] {
                this->redirect.space.xcb_cursors.clear();
            });
        }
    }

    QImage image() const override
    {
        return cursor_image->image();
    }

    QPoint hotspot() const override
    {
        return cursor_image->hotSpot();
    }

    void mark_as_rendered() override
    {
        cursor_image->markAsRendered();
    }

    std::pair<QImage, QPoint> platform_image() const
    {
        return {image(), hotspot()};
    }

    Redirect& redirect;
    std::unique_ptr<wayland::cursor_image<type>> cursor_image;

protected:
    void do_set_pos() override
    {
        redirect.warp_pointer(current_pos(), redirect.platform.base.server->seat()->timestamp());
        slot_pos_changed(redirect.globalPointer());
        Q_EMIT pos_changed(current_pos());
    }

    void do_start_image_tracking() override
    {
        QObject::connect(cursor_image->qobject.get(),
                         &cursor_image_qobject::changed,
                         this,
                         &cursor::image_changed);
    }

    void do_stop_image_tracking() override
    {
        QObject::disconnect(cursor_image->qobject.get(),
                            &cursor_image_qobject::changed,
                            this,
                            &cursor::image_changed);
    }

private:
    Qt::KeyboardModifiers get_keyboard_modifiers()
    {
        return xkb::get_active_keyboard_modifiers(redirect.platform);
    }

    void slot_pos_changed(const QPointF& pos)
    {
        auto const oldPos = current_pos();
        update_pos(pos.toPoint());

        auto mods = get_keyboard_modifiers();
        Q_EMIT mouse_changed(pos.toPoint(), oldPos, m_currentButtons, m_currentButtons, mods, mods);
    }

    void slot_pointer_button_changed()
    {
        Qt::MouseButtons const oldButtons = m_currentButtons;
        m_currentButtons = redirect.qtButtonStates();

        auto const pos = current_pos();
        auto mods = get_keyboard_modifiers();

        Q_EMIT mouse_changed(pos, pos, m_currentButtons, oldButtons, mods, mods);
    }

    void slot_modifiers_changed(Qt::KeyboardModifiers mods, Qt::KeyboardModifiers oldMods)
    {
        Q_EMIT mouse_changed(
            current_pos(), current_pos(), m_currentButtons, m_currentButtons, mods, oldMods);
    }

    Qt::MouseButtons m_currentButtons{Qt::NoButton};
};

}
