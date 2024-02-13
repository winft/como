/*
SPDX-FileCopyrightText: 2006 Lubos Lunak <l.lunak@kde.org>
SPDX-FileCopyrightText: 2010 Jorge Mata <matamax123@gmail.com>
SPDX-FileCopyrightText: 2018 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef KWIN_TRACKMOUSE_H
#define KWIN_TRACKMOUSE_H

#include <render/effect/interface/effect.h>

#include <memory>

class QAction;

namespace como
{

class GLTexture;

class TrackMouseEffect : public Effect
{
    Q_OBJECT
    Q_PROPERTY(Qt::KeyboardModifiers modifiers READ modifiers)
    Q_PROPERTY(bool mousePolling READ isMousePolling)
public:
    TrackMouseEffect();
    ~TrackMouseEffect() override;
    void prePaintScreen(effect::screen_prepaint_data& data) override;
    void paintScreen(effect::screen_paint_data& data) override;
    void postPaintScreen() override;
    void reconfigure(ReconfigureFlags) override;
    bool isActive() const override;

    // for properties
    Qt::KeyboardModifiers modifiers() const
    {
        return m_modifiers;
    }
    bool isMousePolling() const
    {
        return m_mousePolling;
    }
private Q_SLOTS:
    void toggle();
    void slotMouseChanged(const QPoint& pos,
                          const QPoint& old,
                          Qt::MouseButtons buttons,
                          Qt::MouseButtons oldbuttons,
                          Qt::KeyboardModifiers modifiers,
                          Qt::KeyboardModifiers oldmodifiers);

private:
    bool init();
    void loadTexture();
    QRect m_lastRect[2];
    bool m_mousePolling;
    float m_angle;
    float m_angleBase;
    std::unique_ptr<GLTexture> m_texture[2];
    QAction* m_action;
    QImage m_image[2];
    Qt::KeyboardModifiers m_modifiers;

    enum class State { ActivatedByModifiers, ActivatedByShortcut, Inactive };
    State m_state = State::Inactive;
};

} // namespace

#endif
