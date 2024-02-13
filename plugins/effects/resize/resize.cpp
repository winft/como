/*
SPDX-FileCopyrightText: 2009 Martin Gräßlin <mgraesslin@kde.org>

SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "resize.h"

// KConfigSkeleton
#include "resizeconfig.h"

#include <como/render/effect/interface/effect_window.h>
#include <como/render/effect/interface/effects_handler.h>
#include <como/render/effect/interface/paint_data.h>
#include <como/render/gl/interface/shader.h>
#include <como/render/gl/interface/shader_manager.h>
#include <como/render/gl/interface/vertex_buffer.h>

#include <KColorScheme>

#include <QPainter>
#include <QVector2D>

namespace como
{

ResizeEffect::ResizeEffect()
    : AnimationEffect()
    , m_active(false)
    , m_resizeWindow(nullptr)
{
    ResizeConfig::instance(effects->config());
    reconfigure(ReconfigureAll);

    connect(effects, &EffectsHandler::windowAdded, this, &ResizeEffect::slotWindowAdded);

    auto const windows = effects->stackingOrder();
    for (auto window : windows) {
        slotWindowAdded(window);
    }
}

ResizeEffect::~ResizeEffect()
{
}

void ResizeEffect::prePaintScreen(effect::screen_prepaint_data& data)
{
    if (m_active) {
        data.paint.mask |= PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS;
    }
    AnimationEffect::prePaintScreen(data);
}

void ResizeEffect::prePaintWindow(effect::window_prepaint_data& data)
{
    if (m_active && &data.window == m_resizeWindow) {
        data.paint.mask |= PAINT_WINDOW_TRANSFORMED;
    }
    AnimationEffect::prePaintWindow(data);
}

void ResizeEffect::paintWindow(effect::window_paint_data& data)
{
    if (!m_active || &data.window != m_resizeWindow) {
        AnimationEffect::paintWindow(data);
        return;
    }

    if (m_features & TextureScale) {
        data.paint.geo.translation
            += QVector3D(m_currentGeometry.topLeft() - m_originalGeometry.topLeft());
        data.paint.geo.scale
            *= QVector3D(float(m_currentGeometry.width()) / m_originalGeometry.width(),
                         float(m_currentGeometry.height()) / m_originalGeometry.height(),
                         1);
    }

    effects->paintWindow(data);

    if (m_features & Outline) {
        QRegion intersection = m_originalGeometry.intersected(m_currentGeometry);
        QRegion paintRegion
            = QRegion(m_originalGeometry).united(m_currentGeometry).subtracted(intersection);
        float alpha = 0.8f;
        QColor color = KColorScheme(QPalette::Normal, KColorScheme::Selection).background().color();

        if (effects->isOpenGLCompositing()) {
            GLVertexBuffer* vbo = GLVertexBuffer::streamingBuffer();
            vbo->reset();
            ShaderBinder binder(ShaderTrait::UniformColor);
            binder.shader()->setUniform(GLShader::ModelViewProjectionMatrix, effect::get_mvp(data));
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            color.setAlphaF(alpha);
            binder.shader()->setUniform(GLShader::ColorUniform::Color, color);
            QVector<QVector2D> verts;
            verts.reserve(paintRegion.rectCount() * 12);

            for (const QRect& r : paintRegion) {
                verts.push_back(QVector2D(r.x() + r.width(), r.y()));
                verts.push_back(QVector2D(r.x(), r.y()));
                verts.push_back(QVector2D(r.x(), r.y() + r.height()));
                verts.push_back(QVector2D(r.x(), r.y() + r.height()));
                verts.push_back(QVector2D(r.x() + r.width(), r.y() + r.height()));
                verts.push_back(QVector2D(r.x() + r.width(), r.y()));
            }

            vbo->setVertices(verts);
            vbo->render(GL_TRIANGLES);
            glDisable(GL_BLEND);
        } else {
            // Assume QPainter compositing.
            QPainter* painter = effects->scenePainter();
            painter->save();
            color.setAlphaF(alpha);
            for (const QRect& r : paintRegion) {
                painter->fillRect(r, color);
            }
            painter->restore();
        }
    }
}

void ResizeEffect::reconfigure(ReconfigureFlags)
{
    m_features = 0;
    ResizeConfig::self()->read();
    if (ResizeConfig::textureScale())
        m_features |= TextureScale;
    if (ResizeConfig::outline())
        m_features |= Outline;
}

void ResizeEffect::slotWindowAdded(EffectWindow* w)
{
    connect(w,
            &EffectWindow::windowStartUserMovedResized,
            this,
            &ResizeEffect::slotWindowStartUserMovedResized);
    connect(w,
            &EffectWindow::windowStepUserMovedResized,
            this,
            &ResizeEffect::slotWindowStepUserMovedResized);
    connect(w,
            &EffectWindow::windowFinishUserMovedResized,
            this,
            &ResizeEffect::slotWindowFinishUserMovedResized);
}

void ResizeEffect::slotWindowStartUserMovedResized(EffectWindow* w)
{
    if (w->isUserResize() && !w->isUserMove()) {
        m_active = true;
        m_resizeWindow = w;
        m_originalGeometry = w->frameGeometry();
        m_currentGeometry = w->frameGeometry();
        w->addRepaintFull();
    }
}

void ResizeEffect::slotWindowFinishUserMovedResized(EffectWindow* w)
{
    if (m_active && w == m_resizeWindow) {
        m_active = false;
        m_resizeWindow = nullptr;
        if (m_features & TextureScale)
            animate(w, CrossFadePrevious, 0, 150, FPx2(1.0));
        effects->addRepaintFull();
    }
}

void ResizeEffect::slotWindowStepUserMovedResized(EffectWindow* w, const QRect& geometry)
{
    if (m_active && w == m_resizeWindow) {
        m_currentGeometry = geometry;
        effects->addRepaintFull();
    }
}

} // namespace
