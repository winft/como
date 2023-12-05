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

class GLFramebuffer;
class GLTexture;

namespace scripting
{

class ThumbnailTextureProvider;

class window_thumbnail_source : public QObject
{
    Q_OBJECT

public:
    window_thumbnail_source(QQuickWindow* view, scripting::window* handle, QUuid wId);
    ~window_thumbnail_source() override;

    static std::shared_ptr<window_thumbnail_source>
    getOrCreate(QQuickWindow* window, scripting::window* handle, QUuid wId);

    struct Frame {
        std::shared_ptr<GLTexture> texture;
        GLsync fence;
    };

    Frame acquire();

Q_SIGNALS:
    void changed();

private:
    void update(como::effect::screen_paint_data& data);

    QPointer<QQuickWindow> m_view;
    QPointer<scripting::window> m_handle;

    std::shared_ptr<GLTexture> m_offscreenTexture;
    std::unique_ptr<GLFramebuffer> m_offscreenTarget;
    GLsync m_acquireFence{nullptr};
    bool m_dirty = true;
    QUuid wId;
};

class COMO_EXPORT window_thumbnail_item : public QQuickItem
{
    Q_OBJECT
    Q_PROPERTY(QUuid wId READ wId WRITE setWId NOTIFY wIdChanged)
    Q_PROPERTY(como::scripting::window* client READ client WRITE setClient NOTIFY clientChanged)

public:
    explicit window_thumbnail_item(QQuickItem* parent = nullptr);
    ~window_thumbnail_item() override;

    QUuid wId() const;
    void setWId(const QUuid& wId);

    scripting::window* client() const;
    void setClient(scripting::window* window);

    QSGTextureProvider* textureProvider() const override;
    bool isTextureProvider() const override;
    QSGNode* updatePaintNode(QSGNode* oldNode, QQuickItem::UpdatePaintNodeData*) override;

protected:
    void releaseResources() override;

Q_SIGNALS:
    void wIdChanged();
    void clientChanged();

private:
    QImage fallbackImage() const;
    QRectF paintedRect() const;
    void updateImplicitSize();
    void update_source();
    void reset_source();

    QUuid m_wId;
    QPointer<scripting::window> m_client;

    mutable ThumbnailTextureProvider* m_provider = nullptr;
    std::shared_ptr<window_thumbnail_source> m_source;
};

}
}
