/*
SPDX-FileCopyrightText: 2006 Lubos Lunak <l.lunak@kde.org>
SPDX-FileCopyrightText: 2010 Sebastian Sauer <sebsauer@kdab.com>

SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "zoom.h"

// KConfigSkeleton
#include "zoomconfig.h"

#if HAVE_ACCESSIBILITY
#include "accessibilityintegration.h"
#endif

#include <como/render/effect/interface/effect_screen.h>
#include <como/render/effect/interface/effect_window.h>
#include <como/render/effect/interface/effects_handler.h>
#include <como/render/effect/interface/paint_data.h>
#include <como/render/gl/interface/framebuffer.h>
#include <como/render/gl/interface/shader.h>
#include <como/render/gl/interface/shader_manager.h>
#include <como/render/gl/interface/texture.h>
#include <como/render/gl/interface/vertex_buffer.h>

#include <KConfigGroup>
#include <KLocalizedString>
#include <QAction>
#include <QLoggingCategory>
#include <QStyle>
#include <QVector2D>
#include <kstandardaction.h>

Q_LOGGING_CATEGORY(KWIN_ZOOM, "kwin_effect_zoom", QtWarningMsg)

namespace como
{

ZoomEffect::ZoomEffect()
    : Effect()
    , zoom(1)
    , target_zoom(1)
    , polling(false)
    , zoomFactor(1.25)
    , mouseTracking(MouseTrackingProportional)
    , mousePointer(MousePointerScale)
    , focusDelay(350) // in milliseconds
    , isMouseHidden(false)
    , xMove(0)
    , yMove(0)
    , moveFactor(20.0)
    , lastPresentTime(std::chrono::milliseconds::zero())
{
    ZoomConfig::instance(effects->config());
    QAction* a = nullptr;
    a = KStandardAction::zoomIn(this, SLOT(zoomIn()), this);
    effects->registerGlobalShortcutAndDefault({Qt::META | Qt::Key_Plus, Qt::META | Qt::Key_Equal},
                                              a);
    effects->registerAxisShortcut(Qt::ControlModifier | Qt::MetaModifier, PointerAxisDown, a);

    a = KStandardAction::zoomOut(this, SLOT(zoomOut()), this);
    effects->registerGlobalShortcutAndDefault({static_cast<Qt::Key>(Qt::META) + Qt::Key_Minus}, a);
    effects->registerAxisShortcut(Qt::ControlModifier | Qt::MetaModifier, PointerAxisUp, a);

    a = KStandardAction::actualSize(this, SLOT(actualSize()), this);
    effects->registerGlobalShortcutAndDefault({static_cast<Qt::Key>(Qt::META) + Qt::Key_0}, a);

    a = new QAction(this);
    a->setObjectName(QStringLiteral("MoveZoomLeft"));
    a->setText(i18n("Move Zoomed Area to Left"));
    effects->registerGlobalShortcutAndDefault({}, a);
    connect(a, &QAction::triggered, this, &ZoomEffect::moveZoomLeft);

    a = new QAction(this);
    a->setObjectName(QStringLiteral("MoveZoomRight"));
    a->setText(i18n("Move Zoomed Area to Right"));
    effects->registerGlobalShortcutAndDefault({}, a);
    connect(a, &QAction::triggered, this, &ZoomEffect::moveZoomRight);

    a = new QAction(this);
    a->setObjectName(QStringLiteral("MoveZoomUp"));
    a->setText(i18n("Move Zoomed Area Upwards"));
    effects->registerGlobalShortcutAndDefault({}, a);
    connect(a, &QAction::triggered, this, &ZoomEffect::moveZoomUp);

    a = new QAction(this);
    a->setObjectName(QStringLiteral("MoveZoomDown"));
    a->setText(i18n("Move Zoomed Area Downwards"));
    effects->registerGlobalShortcutAndDefault({}, a);
    connect(a, &QAction::triggered, this, &ZoomEffect::moveZoomDown);

    // TODO: these two actions don't belong into the effect. They need to be moved into KWin core
    a = new QAction(this);
    a->setObjectName(QStringLiteral("MoveMouseToFocus"));
    a->setText(i18n("Move Mouse to Focus"));
    effects->registerGlobalShortcutAndDefault({static_cast<Qt::Key>(Qt::META) + Qt::Key_F5}, a);
    connect(a, &QAction::triggered, this, &ZoomEffect::moveMouseToFocus);

    a = new QAction(this);
    a->setObjectName(QStringLiteral("MoveMouseToCenter"));
    a->setText(i18n("Move Mouse to Center"));
    effects->registerGlobalShortcutAndDefault({static_cast<Qt::Key>(Qt::META) + Qt::Key_F6}, a);
    connect(a, &QAction::triggered, this, &ZoomEffect::moveMouseToCenter);

    timeline.setDuration(350);
    timeline.setFrameRange(0, 100);
    connect(&timeline, &QTimeLine::frameChanged, this, &ZoomEffect::timelineFrameChanged);
    connect(effects, &EffectsHandler::mouseChanged, this, &ZoomEffect::slotMouseChanged);
    connect(effects, &EffectsHandler::windowAdded, this, &ZoomEffect::slotWindowAdded);
    connect(effects, &EffectsHandler::screenRemoved, this, &ZoomEffect::slotScreenRemoved);

#if HAVE_ACCESSIBILITY
    if (!effects->waylandDisplay()) {
        // on Wayland, the accessibility integration can cause KWin to hang
        m_accessibilityIntegration = new ZoomAccessibilityIntegration(this);
        connect(m_accessibilityIntegration,
                &ZoomAccessibilityIntegration::focusPointChanged,
                this,
                &ZoomEffect::moveFocus);
    }
#endif

    auto const windows = effects->stackingOrder();
    for (auto w : windows) {
        slotWindowAdded(w);
    }

    source_zoom = -1; // used to trigger initialZoom reading
    reconfigure(ReconfigureAll);
}

ZoomEffect::~ZoomEffect()
{
    // switch off and free resources
    showCursor();
    // Save the zoom value.
    ZoomConfig::setInitialZoom(target_zoom);
    ZoomConfig::self()->save();
}

bool ZoomEffect::isFocusTrackingEnabled() const
{
#if HAVE_ACCESSIBILITY
    return m_accessibilityIntegration && m_accessibilityIntegration->isFocusTrackingEnabled();
#else
    return false;
#endif
}

bool ZoomEffect::isTextCaretTrackingEnabled() const
{
#if HAVE_ACCESSIBILITY
    return m_accessibilityIntegration && m_accessibilityIntegration->isTextCaretTrackingEnabled();
#else
    return false;
#endif
}

GLTexture* ZoomEffect::ensureCursorTexture()
{
    if (!m_cursorTexture || m_cursorTextureDirty) {
        m_cursorTexture.reset();
        m_cursorTextureDirty = false;
        const auto cursor = effects->cursorImage();
        if (!cursor.image.isNull()) {
            m_cursorTexture = std::make_unique<GLTexture>(cursor.image);
            m_cursorTexture->setWrapMode(GL_CLAMP_TO_EDGE);
        }
    }
    return m_cursorTexture.get();
}

void ZoomEffect::markCursorTextureDirty()
{
    m_cursorTextureDirty = true;
}

void ZoomEffect::showCursor()
{
    if (isMouseHidden) {
        disconnect(effects,
                   &EffectsHandler::cursorShapeChanged,
                   this,
                   &ZoomEffect::markCursorTextureDirty);
        // show the previously hidden mouse-pointer again and free the loaded texture/picture.
        effects->showCursor();
        m_cursorTexture.reset();
        isMouseHidden = false;
    }
}

void ZoomEffect::hideCursor()
{
    if (mouseTracking == MouseTrackingProportional && mousePointer == MousePointerKeep)
        return; // don't replace the actual cursor by a static image for no reason.
    if (!isMouseHidden) {
        // try to load the cursor-theme into a OpenGL texture and if successful then hide the
        // mouse-pointer
        GLTexture* texture = nullptr;
        if (effects->isOpenGLCompositing()) {
            texture = ensureCursorTexture();
        }
        if (texture) {
            effects->hideCursor();
            connect(effects,
                    &EffectsHandler::cursorShapeChanged,
                    this,
                    &ZoomEffect::markCursorTextureDirty);
            isMouseHidden = true;
        }
    }
}

void ZoomEffect::reconfigure(ReconfigureFlags)
{
    ZoomConfig::self()->read();
    // On zoom-in and zoom-out change the zoom by the defined zoom-factor.
    zoomFactor = qMax(0.1, ZoomConfig::zoomFactor());
    // Visibility of the mouse-pointer.
    mousePointer = MousePointerType(ZoomConfig::mousePointer());
    // Track moving of the mouse.
    mouseTracking = MouseTrackingType(ZoomConfig::mouseTracking());
#if HAVE_ACCESSIBILITY
    if (m_accessibilityIntegration) {
        // Enable tracking of the focused location.
        m_accessibilityIntegration->setFocusTrackingEnabled(ZoomConfig::enableFocusTracking());
        // Enable tracking of the text caret.
        m_accessibilityIntegration->setTextCaretTrackingEnabled(
            ZoomConfig::enableTextCaretTracking());
    }
#endif
    // The time in milliseconds to wait before a focus-event takes away a mouse-move.
    focusDelay = qMax(uint(0), ZoomConfig::focusDelay());
    // The factor the zoom-area will be moved on touching an edge on push-mode or using the
    // navigation KAction's.
    moveFactor = qMax(0.1, ZoomConfig::moveFactor());
    if (source_zoom < 0) {
        // Load the saved zoom value.
        source_zoom = 1.0;
        target_zoom = ZoomConfig::initialZoom();
        if (target_zoom > 1.0)
            zoomIn(target_zoom);
    } else {
        source_zoom = 1.0;
    }
}

void ZoomEffect::prePaintScreen(effect::screen_prepaint_data& data)
{
    if (zoom != target_zoom) {
        int time = 0;
        if (lastPresentTime.count()) {
            time = (data.present_time - lastPresentTime).count();
        }

        lastPresentTime = data.present_time;

        const float zoomDist = qAbs(target_zoom - source_zoom);
        if (target_zoom > zoom)
            zoom = qMin(zoom + ((zoomDist * time) / animationTime(150 * zoomFactor)), target_zoom);
        else
            zoom = qMax(zoom - ((zoomDist * time) / animationTime(150 * zoomFactor)), target_zoom);
    }

    if (zoom == 1.0) {
        showCursor();
    } else {
        hideCursor();
        data.paint.mask |= PAINT_SCREEN_TRANSFORMED;
    }

    effects->prePaintScreen(data);
}

ZoomEffect::OffscreenData* ZoomEffect::ensureOffscreenData(QRect const& viewport,
                                                           EffectScreen const* screen)
{
    auto const rect = viewport;
    auto const nativeSize = rect.size();

    auto& data = m_offscreenData[effects->waylandDisplay() ? screen : nullptr];
    if (!data.texture || data.texture->size() != nativeSize) {
        data.texture.reset(new GLTexture(GL_RGBA8, nativeSize));
        data.texture->setFilter(GL_LINEAR);
        data.texture->setWrapMode(GL_CLAMP_TO_EDGE);
        data.framebuffer = std::make_unique<GLFramebuffer>(data.texture.get());
    }

    if (!data.vbo || data.viewport != rect) {
        data.vbo.reset(new GLVertexBuffer(GLVertexBuffer::Static));
        data.viewport = rect;

        QVector<GLVertex2D> verts;

        // The v-coordinate is flipped because projection matrix is "flipped."
        verts.push_back(GLVertex2D{
            .position = QVector2D(rect.x() + rect.width(), rect.y()),
            .texcoord = QVector2D(1.0f, 1.0f),
        });
        verts.push_back(GLVertex2D{
            .position = QVector2D(rect.x(), rect.y()),
            .texcoord = QVector2D(0.0, 1.0),
        });
        verts.push_back(GLVertex2D{
            .position = QVector2D(rect.x(), rect.y() + rect.height()),
            .texcoord = QVector2D(0.0, 0.0),
        });
        verts.push_back(GLVertex2D{
            .position = QVector2D(rect.x() + rect.width(), rect.y() + rect.height()),
            .texcoord = QVector2D(1.0, 0.0),
        });
        verts.push_back(GLVertex2D{
            .position = QVector2D(rect.x() + rect.width(), rect.y()),
            .texcoord = QVector2D(1.0, 1.0),
        });
        verts.push_back(GLVertex2D{
            .position = QVector2D(rect.x(), rect.y() + rect.height()),
            .texcoord = QVector2D(0.0, 0.0),
        });

        data.vbo->setVertices(verts);
    }

    return &data;
}

void ZoomEffect::paintScreen(effect::screen_paint_data& data)
{
    auto offscreenData = ensureOffscreenData(data.render.viewport, data.screen);

    QMatrix4x4 projection;
    projection.ortho(QRect{{}, offscreenData->framebuffer->size()});

    // Render the scene in an offscreen texture and then upscale it.
    effect::screen_paint_data offscreen_data{
        .paint = {.mask = data.paint.mask, .region = data.paint.region},
        .render = {.targets = data.render.targets, .projection = projection},
    };

    render::push_framebuffer(data.render, offscreenData->framebuffer.get());
    effects->paintScreen(offscreen_data);
    render::pop_framebuffer(data.render);

    data.paint.geo.scale *= QVector3D(zoom, zoom, 1);
    const QSize screenSize = effects->virtualScreenSize();

    // mouse-tracking allows navigation of the zoom-area using the mouse.
    switch (mouseTracking) {
    case MouseTrackingProportional:
        data.paint.geo.translation.setX(-int(cursorPoint.x() * (zoom - 1.0)));
        data.paint.geo.translation.setY(-int(cursorPoint.y() * (zoom - 1.0)));
        prevPoint = cursorPoint;
        break;
    case MouseTrackingCentred:
        prevPoint = cursorPoint;
        // fall through
    case MouseTrackingDisabled:
        data.paint.geo.translation.setX(
            qMin(0,
                 qMax(int(screenSize.width() - screenSize.width() * zoom),
                      int(screenSize.width() / 2 - prevPoint.x() * zoom))));
        data.paint.geo.translation.setY(
            qMin(0,
                 qMax(int(screenSize.height() - screenSize.height() * zoom),
                      int(screenSize.height() / 2 - prevPoint.y() * zoom))));
        break;
    case MouseTrackingPush: {
        // touching an edge of the screen moves the zoom-area in that direction.
        int x = cursorPoint.x() * zoom - prevPoint.x() * (zoom - 1.0);
        int y = cursorPoint.y() * zoom - prevPoint.y() * (zoom - 1.0);
        int threshold = 4;
        xMove = yMove = 0;
        if (x < threshold) {
            xMove = (x - threshold) / zoom;
        } else if (x + threshold > screenSize.width()) {
            xMove = (x + threshold - screenSize.width()) / zoom;
        }
        if (y < threshold) {
            yMove = (y - threshold) / zoom;
        } else if (y + threshold > screenSize.height()) {
            yMove = (y + threshold - screenSize.height()) / zoom;
        }
        if (xMove) {
            prevPoint.setX(qMax(0, qMin(screenSize.width(), prevPoint.x() + xMove)));
        }
        if (yMove) {
            prevPoint.setY(qMax(0, qMin(screenSize.height(), prevPoint.y() + yMove)));
        }
        data.paint.geo.translation.setX(-int(prevPoint.x() * (zoom - 1.0)));
        data.paint.geo.translation.setY(-int(prevPoint.y() * (zoom - 1.0)));
        break;
    }
    }

    // use the focusPoint if focus tracking is enabled
    if (isFocusTrackingEnabled() || isTextCaretTrackingEnabled()) {
        bool acceptFocus = true;
        if (mouseTracking != MouseTrackingDisabled && focusDelay > 0) {
            // Wait some time for the mouse before doing the switch. This serves as threshold
            // to prevent the focus from jumping around to much while working with the mouse.
            const int msecs = lastMouseEvent.msecsTo(lastFocusEvent);
            acceptFocus = msecs > focusDelay;
        }
        if (acceptFocus) {
            data.paint.geo.translation.setX(-int(focusPoint.x() * (zoom - 1.0)));
            data.paint.geo.translation.setY(-int(focusPoint.y() * (zoom - 1.0)));
            prevPoint = focusPoint;
        }
    }

    // Render transformed offscreen texture.
    glClearColor(0.0, 0.0, 0.0, 0.0);
    glClear(GL_COLOR_BUFFER_BIT);

    QMatrix4x4 matrix;
    matrix.translate(data.paint.geo.translation);
    matrix.scale(data.paint.geo.scale);

    auto shader = ShaderManager::instance()->pushShader(ShaderTrait::MapTexture);
    shader->setUniform(GLShader::ModelViewProjectionMatrix, effect::get_mvp(data) * matrix);
    for (auto& [screen, off_data] : m_offscreenData) {
        off_data.texture->bind();
        off_data.vbo->render(GL_TRIANGLES);
        off_data.texture->unbind();
    }
    ShaderManager::instance()->popShader();

    if (mousePointer != MousePointerHide) {
        // Draw the mouse-texture at the position matching to zoomed-in image of the desktop. Hiding
        // the previous mouse-cursor and drawing our own fake mouse-cursor is needed to be able to
        // scale the mouse-cursor up and to re-position those mouse-cursor to match to the chosen
        // zoom-level.

        GLTexture* cursorTexture = ensureCursorTexture();
        if (cursorTexture) {
            const auto cursor = effects->cursorImage();
            QSize cursorSize = cursor.image.size() / cursor.image.devicePixelRatio();
            if (mousePointer == MousePointerScale) {
                cursorSize *= zoom;
            }

            auto const p = effects->cursorPos() - cursor.hot_spot;
            QRect rect(p * zoom
                           + QPoint(data.paint.geo.translation.x(), data.paint.geo.translation.y()),
                       cursorSize);

            cursorTexture->bind();
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

            auto s = ShaderManager::instance()->pushShader(ShaderTrait::MapTexture);
            auto mvp = effect::get_mvp(data);
            mvp.translate(rect.x(), rect.y());
            s->setUniform(GLShader::ModelViewProjectionMatrix, mvp);

            cursorTexture->render(rect.size());
            ShaderManager::instance()->popShader();
            cursorTexture->unbind();
            glDisable(GL_BLEND);
        }
    }
}

void ZoomEffect::postPaintScreen()
{
    if (zoom == target_zoom) {
        lastPresentTime = std::chrono::milliseconds::zero();
    }

    if (zoom == 1.0 || zoom != target_zoom) {
        // Either animation is running or the zoom effect has stopped.
        effects->addRepaintFull();
    }

    effects->postPaintScreen();
}

void ZoomEffect::zoomIn(double to)
{
    source_zoom = zoom;
    if (to < 0.0)
        target_zoom *= zoomFactor;
    else
        target_zoom = to;
    if (!polling) {
        polling = true;
        effects->startMousePolling();
    }
    cursorPoint = effects->cursorPos();
    if (mouseTracking == MouseTrackingDisabled)
        prevPoint = cursorPoint;
    effects->addRepaintFull();
}

void ZoomEffect::zoomOut()
{
    source_zoom = zoom;
    target_zoom /= zoomFactor;
    if ((zoomFactor > 1 && target_zoom < 1.01) || (zoomFactor < 1 && target_zoom > 0.99)) {
        target_zoom = 1;
        if (polling) {
            polling = false;
            effects->stopMousePolling();
        }
    }
    if (mouseTracking == MouseTrackingDisabled)
        prevPoint = effects->cursorPos();
    effects->addRepaintFull();
}

void ZoomEffect::actualSize()
{
    source_zoom = zoom;
    target_zoom = 1;
    if (polling) {
        polling = false;
        effects->stopMousePolling();
    }
    effects->addRepaintFull();
}

void ZoomEffect::timelineFrameChanged(int /* frame */)
{
    const QSize screenSize = effects->virtualScreenSize();
    prevPoint.setX(qMax(0, qMin(screenSize.width(), prevPoint.x() + xMove)));
    prevPoint.setY(qMax(0, qMin(screenSize.height(), prevPoint.y() + yMove)));
    cursorPoint = prevPoint;
    effects->addRepaintFull();
}

void ZoomEffect::moveZoom(int x, int y)
{
    if (timeline.state() == QTimeLine::Running)
        timeline.stop();

    const QSize screenSize = effects->virtualScreenSize();
    if (x < 0)
        xMove = -qMax(1.0, screenSize.width() / zoom / moveFactor);
    else if (x > 0)
        xMove = qMax(1.0, screenSize.width() / zoom / moveFactor);
    else
        xMove = 0;

    if (y < 0)
        yMove = -qMax(1.0, screenSize.height() / zoom / moveFactor);
    else if (y > 0)
        yMove = qMax(1.0, screenSize.height() / zoom / moveFactor);
    else
        yMove = 0;

    timeline.start();
}

void ZoomEffect::moveZoomLeft()
{
    moveZoom(-1, 0);
}

void ZoomEffect::moveZoomRight()
{
    moveZoom(1, 0);
}

void ZoomEffect::moveZoomUp()
{
    moveZoom(0, -1);
}

void ZoomEffect::moveZoomDown()
{
    moveZoom(0, 1);
}

void ZoomEffect::moveMouseToFocus()
{
    QCursor::setPos(focusPoint.x(), focusPoint.y());
}

void ZoomEffect::moveMouseToCenter()
{
    const QRect r = effects->virtualScreenGeometry();
    QCursor::setPos(r.x() + r.width() / 2, r.y() + r.height() / 2);
}

void ZoomEffect::slotMouseChanged(const QPoint& pos,
                                  const QPoint& old,
                                  Qt::MouseButtons,
                                  Qt::MouseButtons,
                                  Qt::KeyboardModifiers,
                                  Qt::KeyboardModifiers)
{
    if (zoom == 1.0)
        return;
    cursorPoint = pos;
    if (pos != old) {
        lastMouseEvent = QTime::currentTime();
        effects->addRepaintFull();
    }
}

void ZoomEffect::slotWindowAdded(EffectWindow* w)
{
    connect(w, &EffectWindow::windowDamaged, this, &ZoomEffect::slotWindowDamaged);
}

void ZoomEffect::slotWindowDamaged()
{
    if (zoom != 1.0) {
        effects->addRepaintFull();
    }
}

void ZoomEffect::slotScreenRemoved(EffectScreen* screen)
{
    if (auto it = m_offscreenData.find(screen); it != m_offscreenData.end()) {
        effects->makeOpenGLContextCurrent();
        m_offscreenData.erase(it);
    }
}

void ZoomEffect::moveFocus(const QPoint& point)
{
    if (zoom == 1.0)
        return;
    focusPoint = point;
    lastFocusEvent = QTime::currentTime();
    effects->addRepaintFull();
}

bool ZoomEffect::isActive() const
{
    return zoom != 1.0 || zoom != target_zoom;
}

int ZoomEffect::requestedEffectChainPosition() const
{
    return 10;
}

qreal ZoomEffect::configuredZoomFactor() const
{
    return zoomFactor;
}

int ZoomEffect::configuredMousePointer() const
{
    return mousePointer;
}

int ZoomEffect::configuredMouseTracking() const
{
    return mouseTracking;
}

int ZoomEffect::configuredFocusDelay() const
{
    return focusDelay;
}

qreal ZoomEffect::configuredMoveFactor() const
{
    return moveFactor;
}

qreal ZoomEffect::targetZoom() const
{
    return target_zoom;
}

} // namespace
