/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>
    SPDX-FileCopyrightText: 2022 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "offscreen_effect.h"

#include "effect_window.h"
#include "effects_handler.h"
#include "paint_data.h"

#include <como/base/logging.h>
#include <como/render/gl/interface/framebuffer.h>
#include <como/render/gl/interface/shader.h>
#include <como/render/gl/interface/shader_manager.h>
#include <como/render/gl/interface/texture.h>
#include <como/render/gl/interface/vertex_buffer.h>

namespace como
{

struct OffscreenData {
    ~OffscreenData()
    {
        QObject::disconnect(windowExpandedGeometryChangedConnection);
        QObject::disconnect(windowDamagedConnection);
    }

    QScopedPointer<GLTexture> texture;
    QScopedPointer<GLFramebuffer> renderTarget;
    bool isDirty = true;
    GLShader* shader = nullptr;
    QMetaObject::Connection windowExpandedGeometryChangedConnection;
    QMetaObject::Connection windowDamagedConnection;
};

class OffscreenEffectPrivate
{
public:
    QHash<EffectWindow const*, OffscreenData*> windows;
    QMetaObject::Connection windowDeletedConnection;

    void paint(GLTexture* texture,
               effect::window_paint_data const& data,
               WindowQuadList const& quads,
               GLShader* offscreenShader);
    void maybeRender(EffectWindow& window,
                     effect::render_data* render_data,
                     OffscreenData* offscreenData);

    bool live = true;
};

OffscreenEffect::OffscreenEffect(QObject* parent)
    : Effect(parent)
    , d(new OffscreenEffectPrivate)
{
}

OffscreenEffect::~OffscreenEffect()
{
    qDeleteAll(d->windows);
}

bool OffscreenEffect::supported()
{
    return effects->isOpenGLCompositing();
}

void OffscreenEffect::setLive(bool live)
{
    Q_ASSERT(d->windows.isEmpty());
    d->live = live;
}

static void allocateOffscreenData(EffectWindow* window, OffscreenData* offscreenData)
{
    const QRect geometry = window->expandedGeometry();
    offscreenData->texture.reset(new GLTexture(GL_RGBA8, geometry.size()));
    offscreenData->texture->setFilter(GL_LINEAR);
    offscreenData->texture->setWrapMode(GL_CLAMP_TO_EDGE);
    offscreenData->renderTarget.reset(new GLFramebuffer(offscreenData->texture.data()));
    offscreenData->isDirty = true;
}

void OffscreenEffect::redirect(EffectWindow* window)
{
    auto& offscreenData = d->windows[window];
    if (offscreenData) {
        return;
    }

    effects->makeOpenGLContextCurrent();
    offscreenData = new OffscreenData;

    offscreenData->windowExpandedGeometryChangedConnection
        = connect(window,
                  &EffectWindow::windowExpandedGeometryChanged,
                  this,
                  &OffscreenEffect::handleWindowGeometryChanged);

    if (d->live) {
        offscreenData->windowDamagedConnection = connect(
            window, &EffectWindow::windowDamaged, this, &OffscreenEffect::handleWindowDamaged);
    }

    allocateOffscreenData(window, offscreenData);

    if (d->windows.count() == 1) {
        setupConnections();
    }

    if (!d->live) {
        effects->makeOpenGLContextCurrent();
        d->maybeRender(*window, nullptr, offscreenData);
    }
}

void OffscreenEffect::unredirect(EffectWindow const* window)
{
    delete d->windows.take(window);
    if (d->windows.isEmpty()) {
        destroyConnections();
    }
}

void OffscreenEffect::apply(effect::window_paint_data& /*data*/, WindowQuadList& /*quads*/)
{
}

void OffscreenEffectPrivate::maybeRender(EffectWindow& window,
                                         effect::render_data* render_data,
                                         OffscreenData* offscreenData)
{
    if (!offscreenData->isDirty) {
        return;
    }

    auto const geometry = window.expandedGeometry();
    assert(geometry.size() == offscreenData->renderTarget->size());

    QMatrix4x4 projection;
    projection.ortho(QRect({0, 0}, geometry.size()));

    QMatrix4x4 view;
    view.translate(-geometry.x(), -geometry.y());

    std::stack<render::framebuffer*> temp_render_targets;

    effect::render_data render{
        .targets = render_data ? render_data->targets : temp_render_targets,
        .view = view,
        .projection = projection,
        .viewport = {{}, geometry.size()},
    };

    render::push_framebuffer(render, offscreenData->renderTarget.data());

    glClearColor(0.0, 0.0, 0.0, 0.0);
    glClear(GL_COLOR_BUFFER_BIT);

    effect::window_paint_data data{
        window,
        {
            .mask = Effect::PAINT_WINDOW_TRANSFORMED | Effect::PAINT_WINDOW_TRANSLUCENT,
            .region = infiniteRegion(),
            .opacity = 1.,
        },
        render,
    };

    effects->drawWindow(data);

    render::pop_framebuffer(data.render);
    offscreenData->isDirty = false;
}

void OffscreenEffectPrivate::paint(GLTexture* texture,
                                   effect::window_paint_data const& data,
                                   WindowQuadList const& quads,
                                   GLShader* offscreenShader)
{
    auto shader = offscreenShader
        ? offscreenShader
        : ShaderManager::instance()->shader(ShaderTrait::MapTexture | ShaderTrait::Modulate
                                            | ShaderTrait::AdjustSaturation);
    ShaderBinder binder(shader);

    const bool indexedQuads = GLVertexBuffer::supportsIndexedQuads();
    const GLenum primitiveType = indexedQuads ? GL_QUADS : GL_TRIANGLES;
    const int verticesPerQuad = indexedQuads ? 4 : 6;

    GLVertexBuffer* vbo = GLVertexBuffer::streamingBuffer();
    vbo->reset();
    vbo->setAttribLayout(std::span(GLVertexBuffer::GLVertex2DLayout), sizeof(GLVertex2D));

    auto map = vbo->map<GLVertex2D>(verticesPerQuad * quads.count());
    if (!map) {
        qCWarning(KWIN_CORE) << "Could not map vertices for offscreen effect";
        return;
    }

    quads.makeInterleavedArrays(primitiveType, *map, texture->matrix(NormalizedCoordinates));
    vbo->unmap();
    vbo->bindArrays();

    auto const rgb = data.paint.brightness * data.paint.opacity;
    auto const a = data.paint.opacity;

    auto mvp = effect::get_mvp(data);
    QMatrix4x4 translate;
    translate.translate(data.window.x(), data.window.y());

    shader->setUniform(GLShader::ModelViewProjectionMatrix, mvp * translate);
    shader->setUniform(GLShader::ModulationConstant, QVector4D(rgb, rgb, rgb, a));
    shader->setUniform(GLShader::Saturation, data.paint.saturation);
    shader->setUniform(GLShader::TextureWidth, texture->width());
    shader->setUniform(GLShader::TextureHeight, texture->height());

    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

    texture->bind();
    vbo->draw(data.render, data.paint.region, primitiveType, 0, verticesPerQuad * quads.count());
    texture->unbind();

    glDisable(GL_BLEND);
    vbo->unbindArrays();
}

void OffscreenEffect::drawWindow(effect::window_paint_data& data)
{
    auto offscreenData = d->windows.value(&data.window);
    if (!offscreenData) {
        effects->drawWindow(data);
        return;
    }

    auto const expandedGeometry = data.window.expandedGeometry();
    auto const frameGeometry = data.window.frameGeometry();

    QRectF visibleRect = expandedGeometry;
    visibleRect.moveTopLeft(expandedGeometry.topLeft() - frameGeometry.topLeft());
    WindowQuad quad(WindowQuadContents);
    quad[0] = WindowVertex(visibleRect.topLeft(), QPointF(0, 0));
    quad[1] = WindowVertex(visibleRect.topRight(), QPointF(1, 0));
    quad[2] = WindowVertex(visibleRect.bottomRight(), QPointF(1, 1));
    quad[3] = WindowVertex(visibleRect.bottomLeft(), QPointF(0, 1));

    WindowQuadList quads;
    quads.append(quad);
    apply(data, quads);

    d->maybeRender(data.window, &data.render, offscreenData);
    d->paint(offscreenData->texture.data(), data, quads, offscreenData->shader);
}

void OffscreenEffect::handleWindowGeometryChanged(EffectWindow* window)
{
    auto offscreenData = d->windows.value(window);
    if (offscreenData) {
        const QRect geometry = window->expandedGeometry();
        if (offscreenData->texture->size() != geometry.size()) {
            effects->makeOpenGLContextCurrent();
            allocateOffscreenData(window, offscreenData);
        }
    }
}

void OffscreenEffect::handleWindowDamaged(EffectWindow* window)
{
    if (auto offscreenData = d->windows.value(window)) {
        offscreenData->isDirty = true;
    }
}

void OffscreenEffect::setShader(EffectWindow const& window, GLShader* shader)
{
    auto offscreenData = d->windows.value(&window);
    if (offscreenData) {
        offscreenData->shader = shader;
    }
}

void OffscreenEffect::handleWindowDeleted(EffectWindow* window)
{
    effects->makeOpenGLContextCurrent();
    unredirect(window);
}

void OffscreenEffect::setupConnections()
{
    d->windowDeletedConnection = connect(
        effects, &EffectsHandler::windowDeleted, this, &OffscreenEffect::handleWindowDeleted);
}

void OffscreenEffect::destroyConnections()
{
    disconnect(d->windowDeletedConnection);
    d->windowDeletedConnection = {};
}

}
