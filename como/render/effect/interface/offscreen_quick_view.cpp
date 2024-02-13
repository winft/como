/*
    SPDX-FileCopyrightText: 2019 David Edmundson <davidedmundson@kde.org>
    SPDX-FileCopyrightText: 2022 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "offscreen_quick_view.h"

#include "effects_handler.h"

#include <como/base/logging.h>
#include <como/render/gl/interface/texture.h>

#include <QGuiApplication>
#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QOpenGLFramebufferObject>
#include <QPointer>
#include <QQmlEngine>
#include <QQuickGraphicsDevice>
#include <QQuickItem>
#include <QQuickOpenGLUtils>
#include <QQuickRenderControl>
#include <QQuickRenderTarget>
#include <QQuickWindow>
#include <QStyleHints>
#include <QTimer>
#include <QTouchEvent>
#include <QWindow>

// for QMutableEventPoint
#include <QtGui/private/qeventpoint_p.h>

namespace como
{

class Q_DECL_HIDDEN OffscreenQuickView::Private
{
public:
    QQuickWindow* m_view;
    QQuickRenderControl* m_renderControl;
    QScopedPointer<QOffscreenSurface> m_offscreenSurface;
    QScopedPointer<QOpenGLContext> m_glcontext;
    QScopedPointer<QOpenGLFramebufferObject> m_fbo;

    QTimer* m_repaintTimer;
    QImage m_image;
    QScopedPointer<GLTexture> m_textureExport;
    // if we should capture a QImage after rendering into our BO.
    // Used for either software QtQuick rendering and nonGL kwin rendering
    bool m_useBlit = false;
    bool m_visible = true;
    bool m_hasAlphaChannel = true;
    bool m_automaticRepaint = true;

    QList<QEventPoint> touchPoints;
    QPointingDevice* touchDevice;

    ulong lastMousePressTime = 0;
    Qt::MouseButton lastMousePressButton = Qt::NoButton;

    void releaseResources();

    void updateTouchState(Qt::TouchPointState state, qint32 id, const QPointF& pos);
};

class Q_DECL_HIDDEN OffscreenQuickScene::Private
{
public:
    Private()
    {
    }

    QScopedPointer<QQmlComponent> qmlComponent;
    QScopedPointer<QQuickItem> quickItem;
};

OffscreenQuickView::OffscreenQuickView(ExportMode exportMode, bool alpha)
    : d(new OffscreenQuickView::Private)
{
    d->m_renderControl = new QQuickRenderControl();

    d->m_view = new QQuickWindow(d->m_renderControl);
    Q_ASSERT(d->m_view->setProperty("_KWIN_WINDOW_IS_OFFSCREEN", true) || true);
    d->m_view->setFlags(Qt::FramelessWindowHint);
    d->m_view->setColor(Qt::transparent);

    d->m_hasAlphaChannel = alpha;
    if (exportMode == ExportMode::Image) {
        d->m_useBlit = true;
    }

    const bool usingGl
        = d->m_view->rendererInterface()->graphicsApi() == QSGRendererInterface::OpenGL;

    // Compositor renderer is not the same as QtQuick renderer
    if (!effects->isOpenGLCompositing()) {
        qCDebug(KWIN_CORE) << "Compositing is not OpenGL";
        d->m_useBlit = true;
    }

    if (!usingGl) {
        qCDebug(KWIN_CORE) << "QtQuick Software rendering mode detected";
        d->m_useBlit = true;
        d->m_renderControl->initialize();
    } else {
        QSurfaceFormat format;
        format.setOption(QSurfaceFormat::ResetNotification);
        format.setDepthBufferSize(16);
        format.setStencilBufferSize(8);
        if (alpha) {
            format.setAlphaBufferSize(8);
        }

        d->m_view->setFormat(format);

        auto share_context = QOpenGLContext::globalShareContext();
        d->m_glcontext.reset(new QOpenGLContext);
        d->m_glcontext->setShareContext(share_context);
        d->m_glcontext->setFormat(format);
        d->m_glcontext->create();

        // and the offscreen surface
        d->m_offscreenSurface.reset(new QOffscreenSurface);
        d->m_offscreenSurface->setFormat(d->m_glcontext->format());
        d->m_offscreenSurface->create();

        d->m_glcontext->makeCurrent(d->m_offscreenSurface.data());
        d->m_view->setGraphicsDevice(QQuickGraphicsDevice::fromOpenGLContext(d->m_glcontext.get()));
        d->m_renderControl->initialize();
        d->m_glcontext->doneCurrent();

        // On Wayland, opengl contexts are implicitly shared.
        if (share_context && !d->m_glcontext->shareContext()) {
            qCDebug(KWIN_CORE)
                << "Failed to create a shared context, falling back to raster rendering";
            // still render via GL, but blit for presentation
            d->m_useBlit = true;
        }
    }

    auto updateSize = [this]() { contentItem()->setSize(d->m_view->size()); };
    updateSize();
    connect(d->m_view, &QWindow::widthChanged, this, updateSize);
    connect(d->m_view, &QWindow::heightChanged, this, updateSize);

    d->m_repaintTimer = new QTimer(this);
    d->m_repaintTimer->setSingleShot(true);
    d->m_repaintTimer->setInterval(10);

    connect(d->m_repaintTimer, &QTimer::timeout, this, &OffscreenQuickView::update);
    connect(d->m_renderControl,
            &QQuickRenderControl::renderRequested,
            this,
            &OffscreenQuickView::handleRenderRequested);
    connect(d->m_renderControl,
            &QQuickRenderControl::sceneChanged,
            this,
            &OffscreenQuickView::handleSceneChanged);

    d->touchDevice = new QPointingDevice({},
                                         {},
                                         QInputDevice::DeviceType::TouchScreen,
                                         {},
                                         QInputDevice::Capability::Position,
                                         10,
                                         {});
}

OffscreenQuickView::~OffscreenQuickView()
{
    disconnect(d->m_renderControl,
               &QQuickRenderControl::renderRequested,
               this,
               &OffscreenQuickView::handleRenderRequested);
    disconnect(d->m_renderControl,
               &QQuickRenderControl::sceneChanged,
               this,
               &OffscreenQuickView::handleSceneChanged);

    if (d->m_glcontext) {
        // close the view whilst we have an active GL context
        d->m_glcontext->makeCurrent(d->m_offscreenSurface.data());
    }

    delete d->m_renderControl; // Always delete render control first.
    d->m_renderControl = nullptr;

    delete d->m_view;
    d->m_view = nullptr;
}

bool OffscreenQuickView::automaticRepaint() const
{
    return d->m_automaticRepaint;
}

void OffscreenQuickView::setAutomaticRepaint(bool set)
{
    if (d->m_automaticRepaint != set) {
        d->m_automaticRepaint = set;

        // If there's an in-flight update, disable it.
        if (!d->m_automaticRepaint) {
            d->m_repaintTimer->stop();
        }
    }
}

void OffscreenQuickView::handleSceneChanged()
{
    if (d->m_automaticRepaint) {
        d->m_repaintTimer->start();
    }
    Q_EMIT sceneChanged();
}

void OffscreenQuickView::handleRenderRequested()
{
    if (d->m_automaticRepaint) {
        d->m_repaintTimer->start();
    }
    Q_EMIT renderRequested();
}

void OffscreenQuickView::update()
{
    if (!d->m_visible) {
        return;
    }
    if (d->m_view->size().isEmpty()) {
        return;
    }

    bool usingGl = d->m_glcontext != nullptr;

    if (usingGl) {
        if (!d->m_glcontext->makeCurrent(d->m_offscreenSurface.data())) {
            // probably a context loss event, kwin is about to reset all the effects anyway
            return;
        }

        const QSize nativeSize = d->m_view->size() * d->m_view->effectiveDevicePixelRatio();
        if (d->m_fbo.isNull() || d->m_fbo->size() != nativeSize) {
            d->m_textureExport.reset(nullptr);

            QOpenGLFramebufferObjectFormat fboFormat;
            fboFormat.setAttachment(QOpenGLFramebufferObject::CombinedDepthStencil);
            fboFormat.setInternalTextureFormat(d->m_hasAlphaChannel ? GL_RGBA8 : GL_RGB8);

            d->m_fbo.reset(new QOpenGLFramebufferObject(nativeSize, fboFormat));
            if (!d->m_fbo->isValid()) {
                d->m_fbo.reset();
                d->m_glcontext->doneCurrent();
                return;
            }
        }
        d->m_view->setRenderTarget(
            QQuickRenderTarget::fromOpenGLTexture(d->m_fbo->texture(), d->m_fbo->size()));
    }

    d->m_renderControl->polishItems();
    d->m_renderControl->beginFrame();
    d->m_renderControl->sync();
    d->m_renderControl->render();
    d->m_renderControl->endFrame();

    if (usingGl) {
        QQuickOpenGLUtils::resetOpenGLState();
    }

    if (d->m_useBlit) {
        if (usingGl) {
            d->m_image = d->m_fbo->toImage();
            d->m_image.setDevicePixelRatio(d->m_view->devicePixelRatio());
        } else {
            d->m_image = d->m_view->grabWindow();
        }
    }

    if (usingGl) {
        QOpenGLFramebufferObject::bindDefault();
        d->m_glcontext->doneCurrent();
    }
    Q_EMIT repaintNeeded();
}

void OffscreenQuickView::forwardMouseEvent(QEvent* e)
{
    if (!d->m_visible) {
        return;
    }
    switch (e->type()) {
    case QEvent::MouseMove:
    case QEvent::MouseButtonPress:
    case QEvent::MouseButtonRelease: {
        QMouseEvent* me = static_cast<QMouseEvent*>(e);
        const QPoint widgetPos = d->m_view->mapFromGlobal(me->pos());
        QMouseEvent cloneEvent(
            me->type(), widgetPos, me->pos(), me->button(), me->buttons(), me->modifiers());
        QCoreApplication::sendEvent(d->m_view, &cloneEvent);
        e->setAccepted(cloneEvent.isAccepted());

        if (e->type() == QEvent::MouseButtonPress) {
            const ulong doubleClickInterval
                = static_cast<ulong>(QGuiApplication::styleHints()->mouseDoubleClickInterval());
            const bool doubleClick = (me->timestamp() - d->lastMousePressTime < doubleClickInterval)
                && me->button() == d->lastMousePressButton;
            d->lastMousePressTime = me->timestamp();
            d->lastMousePressButton = me->button();
            if (doubleClick) {
                d->lastMousePressButton = Qt::NoButton;
                QMouseEvent doubleClickEvent(QEvent::MouseButtonDblClick,
                                             me->localPos(),
                                             me->windowPos(),
                                             me->screenPos(),
                                             me->button(),
                                             me->buttons(),
                                             me->modifiers());
                QCoreApplication::sendEvent(d->m_view, &doubleClickEvent);
            }
        }

        return;
    }
    case QEvent::HoverEnter:
    case QEvent::HoverLeave:
    case QEvent::HoverMove: {
        QHoverEvent* he = static_cast<QHoverEvent*>(e);
        const QPointF widgetPos = d->m_view->mapFromGlobal(he->pos());
        const QPointF oldWidgetPos = d->m_view->mapFromGlobal(he->oldPos());
        QHoverEvent cloneEvent(he->type(), widgetPos, oldWidgetPos, he->modifiers());
        QCoreApplication::sendEvent(d->m_view, &cloneEvent);
        e->setAccepted(cloneEvent.isAccepted());
        return;
    }
    case QEvent::Wheel: {
        QWheelEvent* we = static_cast<QWheelEvent*>(e);
        const QPointF widgetPos = d->m_view->mapFromGlobal(we->position());
        QWheelEvent cloneEvent(widgetPos,
                               we->globalPosition(),
                               we->pixelDelta(),
                               we->angleDelta(),
                               we->buttons(),
                               we->modifiers(),
                               we->phase(),
                               we->inverted());
        QCoreApplication::sendEvent(d->m_view, &cloneEvent);
        e->setAccepted(cloneEvent.isAccepted());
        return;
    }
    default:
        return;
    }
}

void OffscreenQuickView::forwardKeyEvent(QKeyEvent* keyEvent)
{
    if (!d->m_visible) {
        return;
    }
    QCoreApplication::sendEvent(d->m_view, keyEvent);
}

bool OffscreenQuickView::forwardTouchDown(qint32 id, const QPointF& pos, quint32 time)
{
    Q_UNUSED(time)

    d->updateTouchState(Qt::TouchPointPressed, id, pos);

    QTouchEvent event(QEvent::TouchBegin, d->touchDevice, Qt::NoModifier, d->touchPoints);
    QCoreApplication::sendEvent(d->m_view, &event);

    return event.isAccepted();
}

bool OffscreenQuickView::forwardTouchMotion(qint32 id, const QPointF& pos, quint32 time)
{
    Q_UNUSED(time)

    d->updateTouchState(Qt::TouchPointMoved, id, pos);

    QTouchEvent event(QEvent::TouchUpdate, d->touchDevice, Qt::NoModifier, d->touchPoints);
    QCoreApplication::sendEvent(d->m_view, &event);

    return event.isAccepted();
}

bool OffscreenQuickView::forwardTouchUp(qint32 id, quint32 time)
{
    Q_UNUSED(time)

    d->updateTouchState(Qt::TouchPointReleased, id, QPointF{});

    QTouchEvent event(QEvent::TouchEnd, d->touchDevice, Qt::NoModifier, d->touchPoints);
    QCoreApplication::sendEvent(d->m_view, &event);

    return event.isAccepted();
}

QRect OffscreenQuickView::geometry() const
{
    return d->m_view->geometry();
}

void OffscreenQuickView::setOpacity(qreal opacity)
{
    d->m_view->setOpacity(opacity);
}

qreal OffscreenQuickView::opacity() const
{
    return d->m_view->opacity();
}

bool OffscreenQuickView::hasAlphaChannel() const
{
    return d->m_hasAlphaChannel;
}

QQuickItem* OffscreenQuickView::contentItem() const
{
    return d->m_view->contentItem();
}

QQuickWindow* OffscreenQuickView::window() const
{
    return d->m_view;
}

void OffscreenQuickView::setVisible(bool visible)
{
    if (d->m_visible == visible) {
        return;
    }
    d->m_visible = visible;

    if (visible) {
        Q_EMIT d->m_renderControl->renderRequested();
    } else {
        // deferred to not change GL context
        QTimer::singleShot(0, this, [this]() { d->releaseResources(); });
    }
}

bool OffscreenQuickView::isVisible() const
{
    return d->m_visible;
}

void OffscreenQuickView::show()
{
    setVisible(true);
}

void OffscreenQuickView::hide()
{
    setVisible(false);
}

GLTexture* OffscreenQuickView::bufferAsTexture()
{
    if (d->m_useBlit) {
        if (d->m_image.isNull()) {
            return nullptr;
        }
        d->m_textureExport.reset(new GLTexture(d->m_image));
    } else {
        if (!d->m_fbo) {
            return nullptr;
        }
        if (!d->m_textureExport) {
            d->m_textureExport.reset(new GLTexture(
                d->m_fbo->texture(), d->m_fbo->format().internalTextureFormat(), d->m_fbo->size()));
        }
    }
    return d->m_textureExport.data();
}

QImage OffscreenQuickView::bufferAsImage() const
{
    return d->m_image;
}

QSize OffscreenQuickView::size() const
{
    return d->m_view->geometry().size();
}

void OffscreenQuickView::setGeometry(const QRect& rect)
{
    const QRect oldGeometry = d->m_view->geometry();
    d->m_view->setGeometry(rect);
    Q_EMIT geometryChanged(oldGeometry, rect);
}

void OffscreenQuickView::Private::releaseResources()
{
    if (m_glcontext) {
        m_glcontext->makeCurrent(m_offscreenSurface.data());
        m_view->releaseResources();
        m_glcontext->doneCurrent();
    } else {
        m_view->releaseResources();
    }
}

void OffscreenQuickView::Private::updateTouchState(Qt::TouchPointState state,
                                                   qint32 id,
                                                   const QPointF& pos)
{
    // Remove the points that were previously in a released state, since they
    // are no longer relevant. Additionally, reset the state of all remaining
    // points to Stationary so we only have one touch point with a different
    // state.
    touchPoints.erase(std::remove_if(touchPoints.begin(),
                                     touchPoints.end(),
                                     [](auto& point) {
                                         if (point.state() == QEventPoint::Released) {
                                             return true;
                                         }
                                         QMutableEventPoint::setState(point,
                                                                      QEventPoint::Stationary);
                                         return false;
                                     }),
                      touchPoints.end());

    // QtQuick Pointer Handlers incorrectly consider a touch point with ID 0
    // to be an invalid touch point. This has been fixed in Qt 6 but could not
    // be fixed for Qt 5. Instead, we offset kwin's internal IDs with this
    // offset to trick QtQuick into treating them as valid points.
    static const qint32 idOffset = 111;

    // Find the touch point that has changed. This is separate from the above
    // loop because removing the released touch points invalidates iterators.
    auto changed = std::find_if(
        touchPoints.begin(), touchPoints.end(), [id](const QTouchEvent::TouchPoint& point) {
            return point.id() == id + idOffset;
        });

    switch (state) {
    case Qt::TouchPointPressed: {
        if (changed != touchPoints.end()) {
            return;
        }

        QTouchEvent::TouchPoint point;
        QMutableEventPoint::setState(point, QEventPoint::Pressed);
        QMutableEventPoint::setId(point, id + idOffset);
        QMutableEventPoint::setGlobalPosition(point, pos);
        QMutableEventPoint::setScenePosition(point, m_view->mapFromGlobal(pos.toPoint()));
        QMutableEventPoint::setPosition(point, m_view->mapFromGlobal(pos.toPoint()));

        touchPoints.append(point);
    } break;
    case Qt::TouchPointMoved: {
        if (changed == touchPoints.end()) {
            return;
        }

        auto& point = *changed;
        QMutableEventPoint::setGlobalLastPosition(point, point.globalPosition());
        QMutableEventPoint::setState(point, QEventPoint::Updated);
        QMutableEventPoint::setScenePosition(point, m_view->mapFromGlobal(pos.toPoint()));
        QMutableEventPoint::setPosition(point, m_view->mapFromGlobal(pos.toPoint()));
        QMutableEventPoint::setGlobalPosition(point, pos);
    } break;
    case Qt::TouchPointReleased: {
        if (changed == touchPoints.end()) {
            return;
        }

        auto& point = *changed;
        QMutableEventPoint::setGlobalLastPosition(point, point.globalPosition());
        QMutableEventPoint::setState(point, QEventPoint::Released);
    } break;
    default:
        break;
    }

    // The touch state value is used in QTouchEvent and includes all the states
    // that the current touch points are in.
}

OffscreenQuickScene::OffscreenQuickScene(OffscreenQuickView::ExportMode exportMode, bool alpha)
    : OffscreenQuickView(exportMode, alpha)
    , d(new OffscreenQuickScene::Private)
{
}

OffscreenQuickScene::~OffscreenQuickScene() = default;

void OffscreenQuickScene::setSource(const QUrl& source)
{
    setSource(source, QVariantMap());
}

void OffscreenQuickScene::setSource(const QUrl& source, const QVariantMap& initialProperties)
{
    if (!d->qmlComponent) {
        d->qmlComponent.reset(new QQmlComponent(effects->qmlEngine()));
    }

    d->qmlComponent->loadUrl(source);
    if (d->qmlComponent->isError()) {
        qCWarning(KWIN_CORE).nospace()
            << "Failed to load effect quick view " << source << ": " << d->qmlComponent->errors();
        d->qmlComponent.reset();
        return;
    }

    d->quickItem.reset();

    QScopedPointer<QObject> qmlObject(
        d->qmlComponent->createWithInitialProperties(initialProperties));
    QQuickItem* item = qobject_cast<QQuickItem*>(qmlObject.data());
    if (!item) {
        qCWarning(KWIN_CORE) << "Root object of effect quick view" << source
                             << "is not a QQuickItem";
        return;
    }

    qmlObject.take();
    d->quickItem.reset(item);

    item->setParentItem(contentItem());

    auto updateSize = [item, this]() { item->setSize(contentItem()->size()); };
    updateSize();
    connect(contentItem(), &QQuickItem::widthChanged, item, updateSize);
    connect(contentItem(), &QQuickItem::heightChanged, item, updateSize);
}

QQuickItem* OffscreenQuickScene::rootItem() const
{
    return d->quickItem.data();
}

}
