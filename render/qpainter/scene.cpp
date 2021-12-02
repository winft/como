/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2013 Martin Gräßlin <mgraesslin@kde.org>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/
#include "scene.h"

#include "effect_frame.h"
#include "window.h"

#include "base/output.h"
#include "input/cursor.h"
#include "main.h"
#include "platform.h"
#include "render/compositor.h"
#include "render/cursor.h"
#include "screens.h"
#include "toplevel.h"

#include <kwineffectquickview.h>

#include <Wrapland/Server/buffer.h>
#include <Wrapland/Server/surface.h>

#include "decorations/decoratedclient.h"

#include <KDecoration2/Decoration>
#include <QPainter>

#include <cmath>

namespace KWin::render::qpainter
{

scene::scene(qpainter::backend* backend)
    : m_backend(backend)
    , m_painter(new QPainter())
{
}

scene::~scene()
{
}

CompositingType scene::compositingType() const
{
    return QPainterCompositing;
}

bool scene::initFailed() const
{
    return false;
}

void scene::paintGenericScreen(paint_type mask, ScreenPaintData data)
{
    m_painter->save();
    m_painter->translate(data.xTranslation(), data.yTranslation());
    m_painter->scale(data.xScale(), data.yScale());
    render::scene::paintGenericScreen(mask, data);
    m_painter->restore();
}

int64_t scene::paint_output(base::output* output,
                            QRegion damage,
                            std::deque<Toplevel*> const& toplevels,
                            std::chrono::milliseconds presentTime)
{
    QElapsedTimer renderTimer;
    renderTimer.start();

    createStackingOrder(toplevels);

    auto mask = paint_type::none;
    m_backend->prepareRenderingFrame();

    auto const needsFullRepaint = m_backend->needsFullRepaint();
    if (needsFullRepaint) {
        mask |= render::paint_type::screen_background_first;
        damage = screens()->geometry();
    }

    auto const geometry = output->geometry();

    auto buffer = m_backend->bufferForScreen(output);
    if (!buffer || buffer->isNull()) {
        return renderTimer.nsecsElapsed();
    }

    m_painter->begin(buffer);
    m_painter->save();
    m_painter->setWindow(geometry);

    repaint_output = output;
    QRegion updateRegion, validRegion;

    paintScreen(
        mask, damage.intersected(geometry), QRegion(), &updateRegion, &validRegion, presentTime);
    paintCursor();

    m_painter->restore();
    m_painter->end();

    m_backend->present(output, updateRegion);

    clearStackingOrder();
    Q_EMIT frameRendered();

    return renderTimer.nsecsElapsed();
}

void scene::paintBackground(QRegion region)
{
    m_painter->setBrush(Qt::black);
    for (const QRect& rect : region) {
        m_painter->drawRect(rect);
    }
}

void scene::paintCursor()
{
    auto cursor = render::compositor::self()->software_cursor.get();
    if (!cursor->enabled) {
        return;
    }
    auto const img = cursor->image();
    if (img.isNull()) {
        return;
    }
    auto const cursorPos = input::get_cursor()->pos();
    auto const hotspot = cursor->hotspot();
    m_painter->drawImage(cursorPos - hotspot, img);
    cursor->mark_as_rendered();
}

void scene::paintEffectQuickView(EffectQuickView* w)
{
    QPainter* painter = effects->scenePainter();
    const QImage buffer = w->bufferAsImage();
    if (buffer.isNull()) {
        return;
    }
    painter->drawImage(w->geometry(), buffer);
}

render::window* scene::createWindow(Toplevel* toplevel)
{
    return new window(this, toplevel);
}

render::effect_frame* scene::createEffectFrame(effect_frame_impl* frame)
{
    return new effect_frame(frame, this);
}

render::shadow* scene::createShadow(Toplevel* toplevel)
{
    return new shadow(toplevel);
}

Decoration::Renderer* scene::createDecorationRenderer(Decoration::DecoratedClientImpl* impl)
{
    return new deco_renderer(impl);
}

void scene::handle_screen_geometry_change(QSize const& size)
{
    m_backend->screenGeometryChanged(size);
}

QImage* scene::qpainterRenderBuffer() const
{
    return m_backend->buffer();
}

//****************************************
// QPainterShadow
//****************************************
shadow::shadow(Toplevel* toplevel)
    : render::shadow(toplevel)
{
}

shadow::~shadow()
{
}

void shadow::buildQuads()
{
    // Do not draw shadows if window width or window height is less than
    // 5 px. 5 is an arbitrary choice.
    if (topLevel()->size().width() < 5 || topLevel()->size().height() < 5) {
        m_shadowQuads.clear();
        setShadowRegion(QRegion());
        return;
    }

    const QSizeF top(elementSize(shadow_element::top));
    const QSizeF topRight(elementSize(shadow_element::top_right));
    const QSizeF right(elementSize(shadow_element::right));
    const QSizeF bottomRight(elementSize(shadow_element::bottom_right));
    const QSizeF bottom(elementSize(shadow_element::bottom));
    const QSizeF bottomLeft(elementSize(shadow_element::bottom_left));
    const QSizeF left(elementSize(shadow_element::left));
    const QSizeF topLeft(elementSize(shadow_element::top_left));

    const QRectF outerRect(QPointF(-leftOffset(), -topOffset()),
                           QPointF(topLevel()->size().width() + rightOffset(),
                                   topLevel()->size().height() + bottomOffset()));

    const int width = std::max({topLeft.width(), left.width(), bottomLeft.width()})
        + std::max(top.width(), bottom.width())
        + std::max({topRight.width(), right.width(), bottomRight.width()});
    const int height = std::max({topLeft.height(), top.height(), topRight.height()})
        + std::max(left.height(), right.height())
        + std::max({bottomLeft.height(), bottom.height(), bottomRight.height()});

    QRectF topLeftRect(outerRect.topLeft(), topLeft);
    QRectF topRightRect(outerRect.topRight() - QPointF(topRight.width(), 0), topRight);
    QRectF bottomRightRect(
        outerRect.bottomRight() - QPointF(bottomRight.width(), bottomRight.height()), bottomRight);
    QRectF bottomLeftRect(outerRect.bottomLeft() - QPointF(0, bottomLeft.height()), bottomLeft);

    // Re-distribute the corner tiles so no one of them is overlapping with others.
    // By doing this, we assume that shadow's corner tiles are symmetric
    // and it is OK to not draw top/right/bottom/left tile between corners.
    // For example, let's say top-left and top-right tiles are overlapping.
    // In that case, the right side of the top-left tile will be shifted to left,
    // the left side of the top-right tile will shifted to right, and the top
    // tile won't be rendered.
    bool drawTop = true;
    if (topLeftRect.right() >= topRightRect.left()) {
        const float halfOverlap = qAbs(topLeftRect.right() - topRightRect.left()) / 2;
        topLeftRect.setRight(topLeftRect.right() - std::floor(halfOverlap));
        topRightRect.setLeft(topRightRect.left() + std::ceil(halfOverlap));
        drawTop = false;
    }

    bool drawRight = true;
    if (topRightRect.bottom() >= bottomRightRect.top()) {
        const float halfOverlap = qAbs(topRightRect.bottom() - bottomRightRect.top()) / 2;
        topRightRect.setBottom(topRightRect.bottom() - std::floor(halfOverlap));
        bottomRightRect.setTop(bottomRightRect.top() + std::ceil(halfOverlap));
        drawRight = false;
    }

    bool drawBottom = true;
    if (bottomLeftRect.right() >= bottomRightRect.left()) {
        const float halfOverlap = qAbs(bottomLeftRect.right() - bottomRightRect.left()) / 2;
        bottomLeftRect.setRight(bottomLeftRect.right() - std::floor(halfOverlap));
        bottomRightRect.setLeft(bottomRightRect.left() + std::ceil(halfOverlap));
        drawBottom = false;
    }

    bool drawLeft = true;
    if (topLeftRect.bottom() >= bottomLeftRect.top()) {
        const float halfOverlap = qAbs(topLeftRect.bottom() - bottomLeftRect.top()) / 2;
        topLeftRect.setBottom(topLeftRect.bottom() - std::floor(halfOverlap));
        bottomLeftRect.setTop(bottomLeftRect.top() + std::ceil(halfOverlap));
        drawLeft = false;
    }

    qreal tx1 = 0.0, tx2 = 0.0, ty1 = 0.0, ty2 = 0.0;

    m_shadowQuads.clear();

    tx1 = 0.0;
    ty1 = 0.0;
    tx2 = topLeftRect.width();
    ty2 = topLeftRect.height();
    WindowQuad topLeftQuad(WindowQuadShadow);
    topLeftQuad[0] = WindowVertex(topLeftRect.left(), topLeftRect.top(), tx1, ty1);
    topLeftQuad[1] = WindowVertex(topLeftRect.right(), topLeftRect.top(), tx2, ty1);
    topLeftQuad[2] = WindowVertex(topLeftRect.right(), topLeftRect.bottom(), tx2, ty2);
    topLeftQuad[3] = WindowVertex(topLeftRect.left(), topLeftRect.bottom(), tx1, ty2);
    m_shadowQuads.append(topLeftQuad);

    tx1 = width - topRightRect.width();
    ty1 = 0.0;
    tx2 = width;
    ty2 = topRightRect.height();
    WindowQuad topRightQuad(WindowQuadShadow);
    topRightQuad[0] = WindowVertex(topRightRect.left(), topRightRect.top(), tx1, ty1);
    topRightQuad[1] = WindowVertex(topRightRect.right(), topRightRect.top(), tx2, ty1);
    topRightQuad[2] = WindowVertex(topRightRect.right(), topRightRect.bottom(), tx2, ty2);
    topRightQuad[3] = WindowVertex(topRightRect.left(), topRightRect.bottom(), tx1, ty2);
    m_shadowQuads.append(topRightQuad);

    tx1 = width - bottomRightRect.width();
    tx2 = width;
    ty1 = height - bottomRightRect.height();
    ty2 = height;
    WindowQuad bottomRightQuad(WindowQuadShadow);
    bottomRightQuad[0] = WindowVertex(bottomRightRect.left(), bottomRightRect.top(), tx1, ty1);
    bottomRightQuad[1] = WindowVertex(bottomRightRect.right(), bottomRightRect.top(), tx2, ty1);
    bottomRightQuad[2] = WindowVertex(bottomRightRect.right(), bottomRightRect.bottom(), tx2, ty2);
    bottomRightQuad[3] = WindowVertex(bottomRightRect.left(), bottomRightRect.bottom(), tx1, ty2);
    m_shadowQuads.append(bottomRightQuad);

    tx1 = 0.0;
    tx2 = bottomLeftRect.width();
    ty1 = height - bottomLeftRect.height();
    ty2 = height;
    WindowQuad bottomLeftQuad(WindowQuadShadow);
    bottomLeftQuad[0] = WindowVertex(bottomLeftRect.left(), bottomLeftRect.top(), tx1, ty1);
    bottomLeftQuad[1] = WindowVertex(bottomLeftRect.right(), bottomLeftRect.top(), tx2, ty1);
    bottomLeftQuad[2] = WindowVertex(bottomLeftRect.right(), bottomLeftRect.bottom(), tx2, ty2);
    bottomLeftQuad[3] = WindowVertex(bottomLeftRect.left(), bottomLeftRect.bottom(), tx1, ty2);
    m_shadowQuads.append(bottomLeftQuad);

    if (drawTop) {
        QRectF topRect(topLeftRect.topRight(), topRightRect.bottomLeft());
        tx1 = topLeft.width();
        ty1 = 0.0;
        tx2 = width - topRight.width();
        ty2 = topRect.height();
        WindowQuad topQuad(WindowQuadShadow);
        topQuad[0] = WindowVertex(topRect.left(), topRect.top(), tx1, ty1);
        topQuad[1] = WindowVertex(topRect.right(), topRect.top(), tx2, ty1);
        topQuad[2] = WindowVertex(topRect.right(), topRect.bottom(), tx2, ty2);
        topQuad[3] = WindowVertex(topRect.left(), topRect.bottom(), tx1, ty2);
        m_shadowQuads.append(topQuad);
    }

    if (drawRight) {
        QRectF rightRect(topRightRect.bottomLeft(), bottomRightRect.topRight());
        tx1 = width - rightRect.width();
        ty1 = topRight.height();
        tx2 = width;
        ty2 = height - bottomRight.height();
        WindowQuad rightQuad(WindowQuadShadow);
        rightQuad[0] = WindowVertex(rightRect.left(), rightRect.top(), tx1, ty1);
        rightQuad[1] = WindowVertex(rightRect.right(), rightRect.top(), tx2, ty1);
        rightQuad[2] = WindowVertex(rightRect.right(), rightRect.bottom(), tx2, ty2);
        rightQuad[3] = WindowVertex(rightRect.left(), rightRect.bottom(), tx1, ty2);
        m_shadowQuads.append(rightQuad);
    }

    if (drawBottom) {
        QRectF bottomRect(bottomLeftRect.topRight(), bottomRightRect.bottomLeft());
        tx1 = bottomLeft.width();
        ty1 = height - bottomRect.height();
        tx2 = width - bottomRight.width();
        ty2 = height;
        WindowQuad bottomQuad(WindowQuadShadow);
        bottomQuad[0] = WindowVertex(bottomRect.left(), bottomRect.top(), tx1, ty1);
        bottomQuad[1] = WindowVertex(bottomRect.right(), bottomRect.top(), tx2, ty1);
        bottomQuad[2] = WindowVertex(bottomRect.right(), bottomRect.bottom(), tx2, ty2);
        bottomQuad[3] = WindowVertex(bottomRect.left(), bottomRect.bottom(), tx1, ty2);
        m_shadowQuads.append(bottomQuad);
    }

    if (drawLeft) {
        QRectF leftRect(topLeftRect.bottomLeft(), bottomLeftRect.topRight());
        tx1 = 0.0;
        ty1 = topLeft.height();
        tx2 = leftRect.width();
        ty2 = height - bottomRight.height();
        WindowQuad leftQuad(WindowQuadShadow);
        leftQuad[0] = WindowVertex(leftRect.left(), leftRect.top(), tx1, ty1);
        leftQuad[1] = WindowVertex(leftRect.right(), leftRect.top(), tx2, ty1);
        leftQuad[2] = WindowVertex(leftRect.right(), leftRect.bottom(), tx2, ty2);
        leftQuad[3] = WindowVertex(leftRect.left(), leftRect.bottom(), tx1, ty2);
        m_shadowQuads.append(leftQuad);
    }
}

bool shadow::prepareBackend()
{
    if (hasDecorationShadow()) {
        m_texture = decorationShadowImage();
        return true;
    }

    auto& topLeft = shadowPixmap(shadow_element::top_left);
    auto& top = shadowPixmap(shadow_element::top);
    auto& topRight = shadowPixmap(shadow_element::top_right);
    auto& bottomLeft = shadowPixmap(shadow_element::bottom_left);
    auto& bottom = shadowPixmap(shadow_element::bottom);
    auto& bottomRight = shadowPixmap(shadow_element::bottom_right);
    auto& left = shadowPixmap(shadow_element::left);
    auto& right = shadowPixmap(shadow_element::right);

    const int width = std::max({topLeft.width(), left.width(), bottomLeft.width()})
        + std::max(top.width(), bottom.width())
        + std::max({topRight.width(), right.width(), bottomRight.width()});
    const int height = std::max({topLeft.height(), top.height(), topRight.height()})
        + std::max(left.height(), right.height())
        + std::max({bottomLeft.height(), bottom.height(), bottomRight.height()});

    if (width == 0 || height == 0) {
        return false;
    }

    QImage image(width, height, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);

    QPainter painter;
    painter.begin(&image);
    painter.drawPixmap(0, 0, topLeft.width(), topLeft.height(), topLeft);
    painter.drawPixmap(topLeft.width(), 0, top.width(), top.height(), top);
    painter.drawPixmap(width - topRight.width(), 0, topRight.width(), topRight.height(), topRight);
    painter.drawPixmap(
        0, height - bottomLeft.height(), bottomLeft.width(), bottomLeft.height(), bottomLeft);
    painter.drawPixmap(
        bottomLeft.width(), height - bottom.height(), bottom.width(), bottom.height(), bottom);
    painter.drawPixmap(width - bottomRight.width(),
                       height - bottomRight.height(),
                       bottomRight.width(),
                       bottomRight.height(),
                       bottomRight);
    painter.drawPixmap(0, topLeft.height(), left.width(), left.height(), left);
    painter.drawPixmap(
        width - right.width(), topRight.height(), right.width(), right.height(), right);
    painter.end();

    m_texture = image;

    return true;
}

//****************************************
// QPainterDecorationRenderer
//****************************************
deco_renderer::deco_renderer(Decoration::DecoratedClientImpl* client)
    : Renderer(client)
{
    connect(this,
            &Renderer::renderScheduled,
            client->client(),
            static_cast<void (Toplevel::*)(QRegion const&)>(&Toplevel::addRepaint));
}

deco_renderer::~deco_renderer() = default;

QImage deco_renderer::image(deco_renderer::DecorationPart part) const
{
    Q_ASSERT(part != DecorationPart::Count);
    return m_images[int(part)];
}

void deco_renderer::render()
{
    const QRegion scheduled = getScheduled();
    if (scheduled.isEmpty()) {
        return;
    }
    if (areImageSizesDirty()) {
        resizeImages();
        resetImageSizesDirty();
    }

    auto imageSize = [this](DecorationPart part) {
        return m_images[int(part)].size() / m_images[int(part)].devicePixelRatio();
    };

    const QRect top(QPoint(0, 0), imageSize(DecorationPart::Top));
    const QRect left(QPoint(0, top.height()), imageSize(DecorationPart::Left));
    const QRect right(QPoint(top.width() - imageSize(DecorationPart::Right).width(), top.height()),
                      imageSize(DecorationPart::Right));
    const QRect bottom(QPoint(0, left.y() + left.height()), imageSize(DecorationPart::Bottom));

    const QRect geometry = scheduled.boundingRect();
    auto renderPart = [this](const QRect& rect, const QRect& partRect, int index) {
        if (rect.isEmpty()) {
            return;
        }
        QPainter painter(&m_images[index]);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setWindow(
            QRect(partRect.topLeft(), partRect.size() * m_images[index].devicePixelRatio()));
        painter.setClipRect(rect);
        painter.save();
        // clear existing part
        painter.setCompositionMode(QPainter::CompositionMode_Source);
        painter.fillRect(rect, Qt::transparent);
        painter.restore();
        client()->decoration()->paint(&painter, rect);
    };

    renderPart(left.intersected(geometry), left, int(DecorationPart::Left));
    renderPart(top.intersected(geometry), top, int(DecorationPart::Top));
    renderPart(right.intersected(geometry), right, int(DecorationPart::Right));
    renderPart(bottom.intersected(geometry), bottom, int(DecorationPart::Bottom));
}

void deco_renderer::resizeImages()
{
    QRect left, top, right, bottom;
    client()->client()->layoutDecorationRects(left, top, right, bottom);

    auto checkAndCreate = [this](int index, const QSize& size) {
        auto dpr = client()->client()->screenScale();
        if (m_images[index].size() != size * dpr || m_images[index].devicePixelRatio() != dpr) {
            m_images[index] = QImage(size * dpr, QImage::Format_ARGB32_Premultiplied);
            m_images[index].setDevicePixelRatio(dpr);
            m_images[index].fill(Qt::transparent);
        }
    };
    checkAndCreate(int(DecorationPart::Left), left.size());
    checkAndCreate(int(DecorationPart::Right), right.size());
    checkAndCreate(int(DecorationPart::Top), top.size());
    checkAndCreate(int(DecorationPart::Bottom), bottom.size());
}

void deco_renderer::reparent(Toplevel* window)
{
    render();
    Renderer::reparent(window);
}

render::scene* create_scene()
{
    QScopedPointer<qpainter::backend> backend(kwinApp()->platform->createQPainterBackend());
    if (backend.isNull()) {
        return nullptr;
    }
    if (backend->isFailed()) {
        return nullptr;
    }

    auto s = new scene(backend.take());

    if (s && s->initFailed()) {
        delete s;
        s = nullptr;
    }
    return s;
}

}
