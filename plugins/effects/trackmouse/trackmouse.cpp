/*
SPDX-FileCopyrightText: 2006 Lubos Lunak <l.lunak@kde.org>
SPDX-FileCopyrightText: 2010 Jorge Mata <matamax123@gmail.com>
SPDX-FileCopyrightText: 2018 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "trackmouse.h"

// KConfigSkeleton
#include "trackmouseconfig.h"

#include <base/config-como.h>
#include <render/effect/interface/effects_handler.h>
#include <render/effect/interface/paint_data.h>
#include <render/gl/interface/shader.h>
#include <render/gl/interface/shader_manager.h>
#include <render/gl/interface/texture.h>

#include <KLocalizedString>
#include <QAction>
#include <QMatrix4x4>
#include <QPainter>
#include <QTime>
#include <cmath>

namespace como
{

TrackMouseEffect::TrackMouseEffect()
    : m_angle(0)
{
    TrackMouseConfig::instance(effects->config());
    m_angleBase = 90.0;
    m_mousePolling = false;

    m_action = new QAction(this);
    m_action->setObjectName(QStringLiteral("TrackMouse"));
    m_action->setText(i18n("Track mouse"));
    effects->registerGlobalShortcutAndDefault({}, m_action);

    connect(m_action, &QAction::triggered, this, &TrackMouseEffect::toggle);

    connect(effects, &EffectsHandler::mouseChanged, this, &TrackMouseEffect::slotMouseChanged);
    reconfigure(ReconfigureAll);
}

TrackMouseEffect::~TrackMouseEffect()
{
    if (m_mousePolling) {
        effects->stopMousePolling();
    }
}

void TrackMouseEffect::reconfigure(ReconfigureFlags)
{
    m_modifiers = Qt::KeyboardModifiers();
    TrackMouseConfig::self()->read();
    if (TrackMouseConfig::shift())
        m_modifiers |= Qt::ShiftModifier;
    if (TrackMouseConfig::alt())
        m_modifiers |= Qt::AltModifier;
    if (TrackMouseConfig::control())
        m_modifiers |= Qt::ControlModifier;
    if (TrackMouseConfig::meta())
        m_modifiers |= Qt::MetaModifier;

    if (m_modifiers) {
        if (!m_mousePolling)
            effects->startMousePolling();
        m_mousePolling = true;
    } else if (m_mousePolling) {
        effects->stopMousePolling();
        m_mousePolling = false;
    }
}

void TrackMouseEffect::prePaintScreen(effect::screen_prepaint_data& data)
{
    QTime t = QTime::currentTime();
    m_angle = ((t.second() % 4) * m_angleBase) + (t.msec() / 1000.0 * m_angleBase);
    m_lastRect[0].moveCenter(cursorPos());
    m_lastRect[1].moveCenter(cursorPos());
    data.paint.region |= m_lastRect[0].adjusted(-1, -1, 1, 1);

    effects->prePaintScreen(data);
}

void TrackMouseEffect::paintScreen(effect::screen_paint_data& data)
{
    effects->paintScreen(data);

    if (effects->isOpenGLCompositing() && m_texture[0] && m_texture[1]) {
        ShaderBinder binder(ShaderTrait::MapTexture);
        GLShader* shader(binder.shader());
        if (!shader) {
            return;
        }
        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
        const QPointF p = m_lastRect[0].topLeft()
            + QPoint(m_lastRect[0].width() / 2.0, m_lastRect[0].height() / 2.0);
        auto const x = p.x() * data.paint.geo.scale.x() + data.paint.geo.translation.x();
        auto const y = p.y() * data.paint.geo.scale.y() + data.paint.geo.translation.y();

        for (int i = 0; i < 2; ++i) {
            QMatrix4x4 matrix;
            matrix.translate(x, y, 0.0);
            matrix.rotate(i ? -2 * m_angle : m_angle, 0, 0, 1.0);
            matrix.translate(-x, -y, 0.0);
            matrix.translate(m_lastRect[i].x(), m_lastRect[i].y());
            shader->setUniform(GLShader::ModelViewProjectionMatrix,
                               data.render.projection * data.render.view * matrix);
            m_texture[i]->bind();
            m_texture[i]->render(m_lastRect[i].size());
            m_texture[i]->unbind();
        }
        glDisable(GL_BLEND);
    } else if (!m_image[0].isNull() && !m_image[1].isNull()) {
        // Assume QPainter compositing.
        QPainter* painter = effects->scenePainter();
        const QPointF p = m_lastRect[0].topLeft()
            + QPoint(m_lastRect[0].width() / 2.0, m_lastRect[0].height() / 2.0);
        for (int i = 0; i < 2; ++i) {
            painter->save();
            painter->translate(p.x(), p.y());
            painter->rotate(i ? -2 * m_angle : m_angle);
            painter->translate(-p.x(), -p.y());
            painter->drawImage(m_lastRect[i], m_image[i]);
            painter->restore();
        }
    }
}

void TrackMouseEffect::postPaintScreen()
{
    effects->addRepaint(m_lastRect[0].adjusted(-1, -1, 1, 1));
    effects->postPaintScreen();
}

bool TrackMouseEffect::init()
{
    effects->makeOpenGLContextCurrent();
    if (!m_texture[0] && m_image[0].isNull()) {
        loadTexture();
        if (!m_texture[0] && m_image[0].isNull())
            return false;
    }
    m_lastRect[0].moveCenter(cursorPos());
    m_lastRect[1].moveCenter(cursorPos());
    m_angle = 0;
    return true;
}

void TrackMouseEffect::toggle()
{
    switch (m_state) {
    case State::ActivatedByModifiers:
        m_state = State::ActivatedByShortcut;
        break;

    case State::ActivatedByShortcut:
        m_state = State::Inactive;
        break;

    case State::Inactive:
        if (!init()) {
            return;
        }
        m_state = State::ActivatedByShortcut;
        break;

    default:
        Q_UNREACHABLE();
        break;
    }

    effects->addRepaint(m_lastRect[0].adjusted(-1, -1, 1, 1));
}

void TrackMouseEffect::slotMouseChanged(const QPoint&,
                                        const QPoint&,
                                        Qt::MouseButtons,
                                        Qt::MouseButtons,
                                        Qt::KeyboardModifiers modifiers,
                                        Qt::KeyboardModifiers)
{
    if (!m_mousePolling) { // we didn't ask for it but maybe someone else did...
        return;
    }

    switch (m_state) {
    case State::ActivatedByModifiers:
        if (modifiers == m_modifiers) {
            return;
        }
        m_state = State::Inactive;
        break;

    case State::ActivatedByShortcut:
        return;

    case State::Inactive:
        if (modifiers != m_modifiers) {
            return;
        }
        if (!init()) {
            return;
        }
        m_state = State::ActivatedByModifiers;
        break;

    default:
        Q_UNREACHABLE();
        break;
    }

    effects->addRepaint(m_lastRect[0].adjusted(-1, -1, 1, 1));
}

void TrackMouseEffect::loadTexture()
{
    QString f[2]
        = {QStandardPaths::locate(QStandardPaths::AppDataLocation, QStringLiteral("tm_outer.png")),
           QStandardPaths::locate(QStandardPaths::AppDataLocation, QStringLiteral("tm_inner.png"))};
    if (f[0].isEmpty() || f[1].isEmpty())
        return;

    for (int i = 0; i < 2; ++i) {
        if (effects->isOpenGLCompositing()) {
            QImage img(f[i]);
            m_texture[i] = std::make_unique<GLTexture>(img);
            m_lastRect[i].setSize(img.size());
        } else {
            // Assume QPainter compositing.
            m_image[i] = QImage(f[i]);
            m_lastRect[i].setSize(m_image[i].size());
        }
    }
}

bool TrackMouseEffect::isActive() const
{
    return m_state != State::Inactive;
}

} // namespace
