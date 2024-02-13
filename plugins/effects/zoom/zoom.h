/*
SPDX-FileCopyrightText: 2006 Lubos Lunak <l.lunak@kde.org>
SPDX-FileCopyrightText: 2010 Sebastian Sauer <sebsauer@kdab.com>

SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef KWIN_ZOOM_H
#define KWIN_ZOOM_H

#include <base/config-como.h>
#include <render/effect/interface/effect.h>

#include <QTime>
#include <QTimeLine>
#include <memory>

namespace como
{

#if HAVE_ACCESSIBILITY
class ZoomAccessibilityIntegration;
#endif

class EffectScreen;
class GLFramebuffer;
class GLTexture;
class GLVertexBuffer;

class ZoomEffect : public Effect
{
    Q_OBJECT
    Q_PROPERTY(qreal zoomFactor READ configuredZoomFactor)
    Q_PROPERTY(int mousePointer READ configuredMousePointer)
    Q_PROPERTY(int mouseTracking READ configuredMouseTracking)
    Q_PROPERTY(bool focusTrackingEnabled READ isFocusTrackingEnabled)
    Q_PROPERTY(bool textCaretTrackingEnabled READ isTextCaretTrackingEnabled)
    Q_PROPERTY(int focusDelay READ configuredFocusDelay)
    Q_PROPERTY(qreal moveFactor READ configuredMoveFactor)
    Q_PROPERTY(qreal targetZoom READ targetZoom)
public:
    ZoomEffect();
    ~ZoomEffect() override;
    void reconfigure(ReconfigureFlags flags) override;
    void prePaintScreen(effect::screen_prepaint_data& data) override;
    void paintScreen(effect::screen_paint_data& data) override;
    void postPaintScreen() override;
    bool isActive() const override;
    int requestedEffectChainPosition() const override;
    // for properties
    qreal configuredZoomFactor() const;
    int configuredMousePointer() const;
    int configuredMouseTracking() const;
    bool isFocusTrackingEnabled() const;
    bool isTextCaretTrackingEnabled() const;
    int configuredFocusDelay() const;
    qreal configuredMoveFactor() const;
    qreal targetZoom() const;
private Q_SLOTS:
    inline void zoomIn()
    {
        zoomIn(-1.0);
    };
    void zoomIn(double to);
    void zoomOut();
    void actualSize();
    void moveZoomLeft();
    void moveZoomRight();
    void moveZoomUp();
    void moveZoomDown();
    void moveMouseToFocus();
    void moveMouseToCenter();
    void timelineFrameChanged(int frame);
    void moveFocus(const QPoint& point);
    void slotMouseChanged(const QPoint& pos,
                          const QPoint& old,
                          Qt::MouseButtons buttons,
                          Qt::MouseButtons oldbuttons,
                          Qt::KeyboardModifiers modifiers,
                          Qt::KeyboardModifiers oldmodifiers);
    void slotWindowAdded(EffectWindow* w);
    void slotWindowDamaged();
    void slotScreenRemoved(EffectScreen* screen);

private:
    void showCursor();
    void hideCursor();
    void moveZoom(int x, int y);

private:
    struct OffscreenData {
        std::unique_ptr<GLTexture> texture;
        std::unique_ptr<GLFramebuffer> framebuffer;
        std::unique_ptr<GLVertexBuffer> vbo;
        QRect viewport;
    };

    GLTexture* ensureCursorTexture();
    OffscreenData* ensureOffscreenData(QRect const& viewport, EffectScreen const* screen);
    void markCursorTextureDirty();

#if HAVE_ACCESSIBILITY
    ZoomAccessibilityIntegration* m_accessibilityIntegration = nullptr;
#endif
    double zoom;
    double target_zoom;
    double source_zoom;
    bool polling; // Mouse polling
    double zoomFactor;
    enum MouseTrackingType {
        MouseTrackingProportional = 0,
        MouseTrackingCentred = 1,
        MouseTrackingPush = 2,
        MouseTrackingDisabled = 3
    };
    MouseTrackingType mouseTracking;
    enum MousePointerType { MousePointerScale = 0, MousePointerKeep = 1, MousePointerHide = 2 };
    MousePointerType mousePointer;
    int focusDelay;
    QPoint cursorPoint;
    QPoint focusPoint;
    QPoint prevPoint;
    QTime lastMouseEvent;
    QTime lastFocusEvent;
    std::unique_ptr<GLTexture> m_cursorTexture;
    bool m_cursorTextureDirty = false;
    bool isMouseHidden;
    QTimeLine timeline;
    int xMove, yMove;
    double moveFactor;
    std::chrono::milliseconds lastPresentTime;
    std::map<EffectScreen const*, OffscreenData> m_offscreenData;
};

} // namespace

#endif
