/*
SPDX-FileCopyrightText: 2011 Martin Gräßlin <mgraesslin@kde.org>
SPDX-FileCopyrightText: 2023 Sergio Blanco <seral79@gmail.com>

SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "window_thumbnail_item.h"

#include "scripting_logging.h"
#include "singleton_interface.h"
#include "space.h"

#include <como/render/compositor_qobject.h>
#include <como/render/singleton_interface.h>

#include <como/render/effect/interface/effects_handler.h>
#include <como/render/effect/interface/paint_data.h>
#include <como/render/gl/interface/framebuffer.h>
#include <como/render/gl/interface/texture.h>

#include <QOpenGLContext>
#include <QQuickWindow>
#include <QRunnable>
#include <QSGImageNode>
#include <QSGTextureProvider>

namespace como::scripting
{

namespace
{
bool use_gl_thumbnails()
{
    static bool qt_quick_is_software
        = QStringList({QStringLiteral("software"), QStringLiteral("softwarecontext")})
              .contains(QQuickWindow::sceneGraphBackend());
    return effects && effects->isOpenGLCompositing() && !qt_quick_is_software;
}
}

window_thumbnail_source::window_thumbnail_source(QQuickWindow* view,
                                                 scripting::window* handle,
                                                 QUuid wId)
    : m_view(view)
    , m_handle(handle)
    , wId{wId}
{
    connect(handle, &scripting::window::frameGeometryChanged, this, [this]() {
        m_dirty = true;
        Q_EMIT changed();
    });
    connect(handle, &scripting::window::damaged, this, [this]() {
        m_dirty = true;
        Q_EMIT changed();
    });

    connect(effects, &EffectsHandler::frameRendered, this, &window_thumbnail_source::update);
}

window_thumbnail_source::~window_thumbnail_source()
{
    if (!m_offscreenTexture) {
        return;
    }

    if (!QOpenGLContext::currentContext()) {
        effects->makeOpenGLContextCurrent();
    }

    m_offscreenTarget.reset();
    m_offscreenTexture.reset();

    if (m_acquireFence) {
        glDeleteSync(m_acquireFence);
        m_acquireFence = nullptr;
    }
}

std::shared_ptr<window_thumbnail_source>
window_thumbnail_source::getOrCreate(QQuickWindow* window, scripting::window* handle, QUuid wId)
{
    using window_thumbnail_sourceKey = std::pair<QQuickWindow*, scripting::window*>;
    const window_thumbnail_sourceKey key{window, handle};

    static std::map<window_thumbnail_sourceKey, std::weak_ptr<window_thumbnail_source>> sources;
    auto& source = sources[key];
    if (!source.expired()) {
        return source.lock();
    }

    auto s = std::make_shared<window_thumbnail_source>(window, handle, wId);
    source = s;

    QObject::connect(handle, &scripting::window::destroyed, [key]() { sources.erase(key); });
    QObject::connect(window, &QQuickWindow::destroyed, [key]() { sources.erase(key); });
    return s;
}

window_thumbnail_source::Frame window_thumbnail_source::acquire()
{
    return Frame{
        .texture = m_offscreenTexture,
        .fence = std::exchange(m_acquireFence, nullptr),
    };
}

void window_thumbnail_source::update(effect::screen_paint_data& data)
{
    if (m_acquireFence || !m_dirty || !m_handle) {
        return;
    }
    Q_ASSERT(m_view);

    auto const geometry = m_handle->visibleRect();
    auto const dpi = m_view->devicePixelRatio();
    auto const textureSize = dpi * geometry.size();

    if (!m_offscreenTexture || m_offscreenTexture->size() != textureSize) {
        m_offscreenTexture.reset(new GLTexture(GL_RGBA8, textureSize));
        m_offscreenTexture->setFilter(GL_LINEAR);
        m_offscreenTexture->setWrapMode(GL_CLAMP_TO_EDGE);
        m_offscreenTarget.reset(new GLFramebuffer(m_offscreenTexture.get()));
    }

    QMatrix4x4 view;
    view.ortho(geometry.x(),
               geometry.x() + geometry.width(),
               geometry.y(),
               geometry.y() + geometry.height(),
               -1,
               1);

    QMatrix4x4 proj;
    proj.scale(dpi);

    auto effectWindow = effects->findWindow(wId);

    effect::window_paint_data win_data{
        *effectWindow,
        {
            .mask = Effect::PAINT_WINDOW_TRANSFORMED,
            .region = infiniteRegion(),
        },
        {
            .targets = data.render.targets,
            .view = view,
            .projection = proj,
            .viewport = geometry,
        },
    };

    render::push_framebuffer(win_data.render, m_offscreenTarget.get());
    glClearColor(0.0, 0.0, 0.0, 0.0);
    glClear(GL_COLOR_BUFFER_BIT);

    // The thumbnail must be rendered using kwin's opengl context as VAOs are not
    // shared across contexts. Unfortunately, this also introduces a latency of 1
    // frame, which is not ideal, but it is acceptable for things such as thumbnails.
    effects->drawWindow(win_data);
    render::pop_framebuffer(win_data.render);

    // The fence is needed to avoid the case where qtquick renderer starts using
    // the texture while all rendering commands to it haven't completed yet.
    m_dirty = false;
    m_acquireFence = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);

    Q_EMIT changed();
}

class ThumbnailTextureProvider : public QSGTextureProvider
{
public:
    explicit ThumbnailTextureProvider(QQuickWindow* window);

    QSGTexture* texture() const override;
    void setTexture(std::shared_ptr<GLTexture> const& nativeTexture);
    void setTexture(QSGTexture* texture);

private:
    QQuickWindow* m_window;
    std::shared_ptr<GLTexture> m_nativeTexture;
    QScopedPointer<QSGTexture> m_texture;
};

ThumbnailTextureProvider::ThumbnailTextureProvider(QQuickWindow* window)
    : m_window(window)
{
}

QSGTexture* ThumbnailTextureProvider::texture() const
{
    return m_texture.data();
}

void ThumbnailTextureProvider::setTexture(std::shared_ptr<GLTexture> const& nativeTexture)
{
    if (m_nativeTexture != nativeTexture) {
        auto const textureId = nativeTexture->texture();
        m_nativeTexture = nativeTexture;
        m_texture.reset(QNativeInterface::QSGOpenGLTexture::fromNative(
            textureId, m_window, nativeTexture->size(), QQuickWindow::TextureHasAlphaChannel));
        m_texture->setFiltering(QSGTexture::Linear);
        m_texture->setHorizontalWrapMode(QSGTexture::ClampToEdge);
        m_texture->setVerticalWrapMode(QSGTexture::ClampToEdge);
    }

    // The textureChanged signal must be emitted also if only texture data changes.
    Q_EMIT textureChanged();
}

void ThumbnailTextureProvider::setTexture(QSGTexture* texture)
{
    m_nativeTexture = nullptr;
    m_texture.reset(texture);
    Q_EMIT textureChanged();
}

class ThumbnailTextureProviderCleanupJob : public QRunnable
{
public:
    explicit ThumbnailTextureProviderCleanupJob(ThumbnailTextureProvider* provider)
        : m_provider(provider)
    {
    }

    void run() override
    {
        m_provider.reset();
    }

private:
    QScopedPointer<ThumbnailTextureProvider> m_provider;
};

window_thumbnail_item::window_thumbnail_item(QQuickItem* parent)
    : QQuickItem(parent)
{
    setFlag(ItemHasContents);

    connect(render::singleton_interface::compositor,
            &render::compositor_qobject::aboutToToggleCompositing,
            this,
            &window_thumbnail_item::reset_source);
    connect(render::singleton_interface::compositor,
            &render::compositor_qobject::compositingToggled,
            this,
            &window_thumbnail_item::update_source);
    connect(this, &QQuickItem::windowChanged, this, &window_thumbnail_item::update_source);
}

window_thumbnail_item::~window_thumbnail_item()
{
    if (m_provider) {
        if (window()) {
            window()->scheduleRenderJob(new ThumbnailTextureProviderCleanupJob(m_provider),
                                        QQuickWindow::AfterSynchronizingStage);
        } else {
            qCCritical(KWIN_SCRIPTING)
                << "Can't destroy thumbnail texture provider because window is null";
        }
    }
}

void window_thumbnail_item::releaseResources()
{
    if (m_provider) {
        window()->scheduleRenderJob(new ThumbnailTextureProviderCleanupJob(m_provider),
                                    QQuickWindow::AfterSynchronizingStage);
        m_provider = nullptr;
    }
}

bool window_thumbnail_item::isTextureProvider() const
{
    return true;
}

QSGTextureProvider* window_thumbnail_item::textureProvider() const
{
    if (QQuickItem::isTextureProvider()) {
        return QQuickItem::textureProvider();
    }
    if (!m_provider) {
        m_provider = new ThumbnailTextureProvider(window());
    }
    return m_provider;
}

void window_thumbnail_item::reset_source()
{
    m_source.reset();
}

void window_thumbnail_item::update_source()
{
    if (use_gl_thumbnails() && window() && m_client) {
        m_source = window_thumbnail_source::getOrCreate(window(), m_client, m_wId);
        connect(m_source.get(),
                &window_thumbnail_source::changed,
                this,
                &window_thumbnail_item::update);
    } else {
        m_source.reset();
    }
}

QSGNode* window_thumbnail_item::updatePaintNode(QSGNode* oldNode, QQuickItem::UpdatePaintNodeData*)
{
    if (effects) {
        if (!m_source) {
            return oldNode;
        }

        auto [texture, acquireFence] = m_source->acquire();
        if (!texture) {
            return oldNode;
        }

        // Wait for rendering commands to the offscreen texture complete if there are any.
        if (acquireFence) {
            glWaitSync(acquireFence, 0, GL_TIMEOUT_IGNORED);
            glDeleteSync(acquireFence);
        }

        if (!m_provider) {
            m_provider = new ThumbnailTextureProvider(window());
        }
        m_provider->setTexture(texture);
    } else {
        if (!m_provider) {
            m_provider = new ThumbnailTextureProvider(window());
        }

        auto const placeholderImage = fallbackImage();
        m_provider->setTexture(window()->createTextureFromImage(placeholderImage));
    }

    QSGImageNode* node = static_cast<QSGImageNode*>(oldNode);
    if (!node) {
        node = window()->createImageNode();
        node->setFiltering(QSGTexture::Linear);
    }
    node->setTexture(m_provider->texture());
    node->setTextureCoordinatesTransform(QSGImageNode::NoTransform);
    node->setRect(paintedRect());

    return node;
}

QUuid window_thumbnail_item::wId() const
{
    return m_wId;
}

scripting::window* find_controlled_window(QUuid const& wId)
{
    auto const windows = singleton_interface::qt_script_space->windowList();
    for (auto win : windows) {
        if (win->internalId() == wId) {
            return win;
        }
    }
    return nullptr;
}

void window_thumbnail_item::setWId(const QUuid& wId)
{
    if (m_wId == wId) {
        return;
    }
    m_wId = wId;
    if (!m_wId.isNull()) {
        setClient(find_controlled_window(m_wId));
    } else {
        if (m_client) {
            m_client = nullptr;
            update_source();
            updateImplicitSize();
            Q_EMIT clientChanged();
        }
    }
    Q_EMIT wIdChanged();
}

scripting::window* window_thumbnail_item::client() const
{
    return m_client;
}

void window_thumbnail_item::setClient(scripting::window* client)
{
    if (m_client == client) {
        return;
    }
    if (m_client) {
        disconnect(m_client,
                   &scripting::window::frameGeometryChanged,
                   this,
                   &window_thumbnail_item::updateImplicitSize);
    }
    m_client = client;
    if (m_client) {
        connect(m_client,
                &scripting::window::frameGeometryChanged,
                this,
                &window_thumbnail_item::updateImplicitSize);
        setWId(m_client->internalId());
    } else {
        setWId(QUuid());
    }
    update_source();
    updateImplicitSize();
    Q_EMIT clientChanged();
}

void window_thumbnail_item::updateImplicitSize()
{
    QSize frameSize;
    if (m_client) {
        frameSize = m_client->frameGeometry().size();
    }
    setImplicitSize(frameSize.width(), frameSize.height());
}

QImage window_thumbnail_item::fallbackImage() const
{
    if (m_client) {
        return m_client->icon().pixmap(boundingRect().size().toSize()).toImage();
    }
    return QImage();
}

static QRectF centeredSize(const QRectF& boundingRect, const QSizeF& size)
{
    auto const scaled = size.scaled(boundingRect.size(), Qt::KeepAspectRatio);
    auto const x = boundingRect.x() + (boundingRect.width() - scaled.width()) / 2;
    auto const y = boundingRect.y() + (boundingRect.height() - scaled.height()) / 2;
    return QRectF(QPointF(x, y), scaled);
}

QRectF window_thumbnail_item::paintedRect() const
{
    if (!m_client) {
        return QRectF();
    }
    if (!effects) {
        auto const iconSize = m_client->icon().actualSize(boundingRect().size().toSize());
        return centeredSize(boundingRect(), iconSize);
    }

    auto const visibleGeometry = m_client->visibleRect();
    auto const frameGeometry = m_client->frameGeometry();
    auto const scaled
        = QSizeF(frameGeometry.size()).scaled(boundingRect().size(), Qt::KeepAspectRatio);

    auto const xScale = scaled.width() / frameGeometry.width();
    auto const yScale = scaled.height() / frameGeometry.height();

    QRectF paintedRect(boundingRect().x() + (boundingRect().width() - scaled.width()) / 2,
                       boundingRect().y() + (boundingRect().height() - scaled.height()) / 2,
                       visibleGeometry.width() * xScale,
                       visibleGeometry.height() * yScale);

    paintedRect.moveLeft(paintedRect.x() + (visibleGeometry.x() - frameGeometry.x()) * xScale);
    paintedRect.moveTop(paintedRect.y() + (visibleGeometry.y() - frameGeometry.y()) * yScale);

    return paintedRect;
}

}
