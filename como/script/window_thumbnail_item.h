/*
SPDX-FileCopyrightText: 2011 Martin Gräßlin <mgraesslin@kde.org>

SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "window.h"

#include "como_export.h"
#include <como/render/effect/interface/paint_data.h>

#include <QQuickItem>
#include <QUuid>

#include <epoxy/gl.h>

namespace como
{

class EffectWindow;
class GLFramebuffer;
class GLTexture;

namespace scripting
{
class ThumbnailTextureProvider;

class COMO_EXPORT window_thumbnail_item : public QQuickItem
{
    Q_OBJECT
    Q_PROPERTY(QUuid wId READ wId WRITE setWId NOTIFY wIdChanged)
    Q_PROPERTY(como::scripting::window* client READ client WRITE setClient NOTIFY clientChanged)
    Q_PROPERTY(QSize sourceSize READ sourceSize WRITE setSourceSize NOTIFY sourceSizeChanged)

public:
    explicit window_thumbnail_item(QQuickItem* parent = nullptr);
    ~window_thumbnail_item() override;

    QUuid wId() const;
    void setWId(const QUuid& wId);

    scripting::window* client() const;
    void setClient(scripting::window* window);

    QSize sourceSize() const;
    void setSourceSize(const QSize& sourceSize);

    QSGTextureProvider* textureProvider() const override;
    bool isTextureProvider() const override;
    QSGNode* updatePaintNode(QSGNode* oldNode, QQuickItem::UpdatePaintNodeData*) override;

protected:
    void releaseResources() override;

Q_SIGNALS:
    void wIdChanged();
    void clientChanged();
    void sourceSizeChanged();

private:
    bool use_gl_thumbnails() const;
    QImage fallbackImage() const;
    QRectF paintedRect() const;
    void invalidateOffscreenTexture();
    void updateOffscreenTexture(como::effect::screen_paint_data& data);
    void destroyOffscreenTexture();
    void updateImplicitSize();
    void update_render_notifier();

    QSize m_sourceSize;
    QUuid m_wId;
    QPointer<scripting::window> m_client;
    bool m_dirty = false;

    mutable ThumbnailTextureProvider* m_provider = nullptr;
    QSharedPointer<GLTexture> m_offscreenTexture;
    QScopedPointer<GLFramebuffer> m_offscreenTarget;
    GLsync m_acquireFence{nullptr};
    qreal m_devicePixelRatio = 1;

    QMetaObject::Connection render_notifier;
};

}
}
