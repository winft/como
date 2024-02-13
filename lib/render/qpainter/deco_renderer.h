/*
    SPDX-FileCopyrightText: 2013 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2021 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "win/deco/renderer.h"

#include <QImage>

namespace como::render::qpainter
{

enum class DecorationPart : int {
    Left,
    Top,
    Right,
    Bottom,
    Count,
};

class deco_render_data : public win::deco::render_data
{
public:
    QImage image(DecorationPart part) const
    {
        assert(part != DecorationPart::Count);
        return images[int(part)];
    }

    QImage images[int(DecorationPart::Count)];
};

class deco_renderer : public win::deco::render_injector
{
public:
    explicit deco_renderer(win::deco::render_window window)
        : win::deco::render_injector(std::move(window))
    {
        this->data = std::make_unique<deco_render_data>();
    }

    void render() override
    {
        auto const scheduled = this->getScheduled();
        if (scheduled.isEmpty()) {
            return;
        }
        if (this->image_size_dirty) {
            resizeImages();
            this->image_size_dirty = false;
        }

        auto imageSize = [this](DecorationPart part) {
            auto& images = get_data().images;
            return images[int(part)].size() / images[int(part)].devicePixelRatio();
        };

        const QRect top(QPoint(0, 0), imageSize(DecorationPart::Top));
        const QRect left(QPoint(0, top.height()), imageSize(DecorationPart::Left));
        const QRect right(
            QPoint(top.width() - imageSize(DecorationPart::Right).width(), top.height()),
            imageSize(DecorationPart::Right));
        const QRect bottom(QPoint(0, left.y() + left.height()), imageSize(DecorationPart::Bottom));

        const QRect geometry = scheduled.boundingRect();
        auto renderPart = [this](const QRect& rect, const QRect& partRect, int index) {
            if (rect.isEmpty()) {
                return;
            }
            auto& data = get_data();
            QPainter painter(&data.images[index]);
            painter.setRenderHint(QPainter::Antialiasing);
            painter.setWindow(
                QRect(partRect.topLeft(), partRect.size() * data.images[index].devicePixelRatio()));
            painter.setClipRect(rect);
            painter.save();
            // clear existing part
            painter.setCompositionMode(QPainter::CompositionMode_Source);
            painter.fillRect(rect, Qt::transparent);
            painter.restore();
            this->window.deco->paint(&painter, rect);
        };

        renderPart(left.intersected(geometry), left, int(DecorationPart::Left));
        renderPart(top.intersected(geometry), top, int(DecorationPart::Top));
        renderPart(right.intersected(geometry), right, int(DecorationPart::Right));
        renderPart(bottom.intersected(geometry), bottom, int(DecorationPart::Bottom));
    }

private:
    deco_render_data& get_data()
    {
        return static_cast<deco_render_data&>(*this->data);
    }

    void resizeImages()
    {
        QRect left, top, right, bottom;
        this->window.layout_rects(left, top, right, bottom);

        auto checkAndCreate = [this](int index, const QSize& size) {
            auto dpr = this->window.scale();
            auto& images = get_data().images;
            if (images[index].size() != size * dpr || images[index].devicePixelRatio() != dpr) {
                images[index] = QImage(size * dpr, QImage::Format_ARGB32_Premultiplied);
                images[index].setDevicePixelRatio(dpr);
                images[index].fill(Qt::transparent);
            }
        };
        checkAndCreate(int(DecorationPart::Left), left.size());
        checkAndCreate(int(DecorationPart::Right), right.size());
        checkAndCreate(int(DecorationPart::Top), top.size());
        checkAndCreate(int(DecorationPart::Bottom), bottom.size());
    }
};

}
