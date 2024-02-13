/*
SPDX-FileCopyrightText: 2007 Lubos Lunak <l.lunak@kde.org>
SPDX-FileCopyrightText: 2011 Martin Gräßlin <mgraesslin@kde.org>

SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef KWIN_MAGNIFIER_H
#define KWIN_MAGNIFIER_H

#include <render/effect/interface/effect.h>

#include <memory>

namespace como
{

class GLFramebuffer;
class GLTexture;

class MagnifierEffect : public Effect
{
    Q_OBJECT
    Q_PROPERTY(QSize magnifierSize READ magnifierSize)
    Q_PROPERTY(qreal targetZoom READ targetZoom)
public:
    MagnifierEffect();
    ~MagnifierEffect() override;
    void reconfigure(ReconfigureFlags) override;
    void prePaintScreen(effect::screen_prepaint_data& data) override;
    void paintScreen(effect::screen_paint_data& data) override;
    void postPaintScreen() override;
    bool isActive() const override;
    static bool supported();

    // for properties
    QSize magnifierSize() const;
    qreal targetZoom() const;
private Q_SLOTS:
    void zoomIn();
    void zoomOut();
    void toggle();
    void slotMouseChanged(const QPoint& pos,
                          const QPoint& old,
                          Qt::MouseButtons buttons,
                          Qt::MouseButtons oldbuttons,
                          Qt::KeyboardModifiers modifiers,
                          Qt::KeyboardModifiers oldmodifiers);
    void slotWindowAdded(EffectWindow* w);
    void slotWindowDamaged();

private:
    QRect magnifierArea(QPoint pos = cursorPos()) const;
    double m_zoom;
    double m_targetZoom;
    bool m_polling; // Mouse polling
    std::chrono::milliseconds m_lastPresentTime;
    QSize m_magnifierSize;
    std::unique_ptr<GLTexture> m_texture;
    std::unique_ptr<GLFramebuffer> m_fbo;
};

} // namespace

#endif
