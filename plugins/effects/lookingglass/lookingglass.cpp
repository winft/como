/*
SPDX-FileCopyrightText: 2007 Rivo Laks <rivolaks@hot.ee>
SPDX-FileCopyrightText: 2007 Christian Nitschkowski <christian.nitschkowski@kdemail.net>

SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "lookingglass.h"

// KConfigSkeleton
#include "lookingglassconfig.h"

#include <como/render/effect/interface/effect_window.h>
#include <como/render/effect/interface/effects_handler.h>
#include <como/render/effect/interface/paint_data.h>
#include <como/render/gl/interface/framebuffer.h>
#include <como/render/gl/interface/platform.h>
#include <como/render/gl/interface/shader.h>
#include <como/render/gl/interface/shader_manager.h>
#include <como/render/gl/interface/texture.h>
#include <como/render/gl/interface/vertex_buffer.h>

#include <KLocalizedString>
#include <KStandardAction>
#include <QAction>
#include <QFile>
#include <QLoggingCategory>
#include <QVector2D>
#include <cmath>
#include <kmessagebox.h>

Q_LOGGING_CATEGORY(KWIN_LOOKINGGLASS, "kwin_effect_lookingglass", QtWarningMsg)

static void ensureResources()
{
    // Must initialize resources manually because the effect is a static lib.
    Q_INIT_RESOURCE(lookingglass);
}

namespace como
{

LookingGlassEffect::LookingGlassEffect()
    : zoom(1.0f)
    , target_zoom(1.0f)
    , polling(false)
    , m_texture(nullptr)
    , m_fbo(nullptr)
    , m_vbo(nullptr)
    , m_shader(nullptr)
    , m_lastPresentTime(std::chrono::milliseconds::zero())
    , m_enabled(false)
    , m_valid(false)
{
    LookingGlassConfig::instance(effects->config());
    QAction* a;
    a = KStandardAction::zoomIn(this, SLOT(zoomIn()), this);
    effects->registerGlobalShortcutAndDefault({static_cast<Qt::Key>(Qt::META) + Qt::Key_Equal}, a);

    a = KStandardAction::zoomOut(this, SLOT(zoomOut()), this);
    effects->registerGlobalShortcutAndDefault({static_cast<Qt::Key>(Qt::META) + Qt::Key_Minus}, a);

    a = KStandardAction::actualSize(this, SLOT(toggle()), this);
    effects->registerGlobalShortcutAndDefault({static_cast<Qt::Key>(Qt::META) + Qt::Key_0}, a);

    connect(effects, &EffectsHandler::mouseChanged, this, &LookingGlassEffect::slotMouseChanged);
    connect(effects, &EffectsHandler::windowAdded, this, &LookingGlassEffect::slotWindowAdded);

    auto const windows = effects->stackingOrder();
    for (auto window : windows) {
        slotWindowAdded(window);
    }

    reconfigure(ReconfigureAll);
}

LookingGlassEffect::~LookingGlassEffect() = default;

bool LookingGlassEffect::supported()
{
    return effects->isOpenGLCompositing();
}

void LookingGlassEffect::reconfigure(ReconfigureFlags)
{
    LookingGlassConfig::self()->read();
    initialradius = LookingGlassConfig::radius();
    radius = initialradius;
    qCDebug(KWIN_LOOKINGGLASS) << "Radius from config:" << radius;
    m_valid = loadData();
}

bool LookingGlassEffect::loadData()
{
    ensureResources();

    const QSize screenSize = effects->virtualScreenSize();
    int texw = screenSize.width();
    int texh = screenSize.height();

    // Create texture and render target
    const int levels = std::log2(qMin(texw, texh)) + 1;
    m_texture = std::make_unique<GLTexture>(GL_RGBA8, texw, texh, levels);
    m_texture->setFilter(GL_LINEAR_MIPMAP_LINEAR);
    m_texture->setWrapMode(GL_CLAMP_TO_EDGE);

    m_fbo = std::make_unique<GLFramebuffer>(m_texture.get());
    if (!m_fbo->valid()) {
        return false;
    }

    m_shader = ShaderManager::instance()->generateShaderFromFile(
        ShaderTrait::MapTexture,
        QString(),
        QStringLiteral(":/effects/lookingglass/shaders/lookingglass.frag"));
    if (m_shader->isValid()) {
        ShaderBinder binder(m_shader.get());
        m_shader->setUniform("u_textureSize", QVector2D(screenSize.width(), screenSize.height()));
    } else {
        qCCritical(KWIN_LOOKINGGLASS) << "The shader failed to load!";
        return false;
    }

    m_vbo = std::make_unique<GLVertexBuffer>(GLVertexBuffer::Static);
    QVector<GLVertex2D> verts;

    verts.push_back(GLVertex2D{
        .position = QVector2D(screenSize.width(), 0.0),
        .texcoord = QVector2D(screenSize.width(), 0.0),
    });
    verts.push_back(GLVertex2D{
        .position = QVector2D(0.0, 0.0),
        .texcoord = QVector2D(0.0, 0.0),
    });
    verts.push_back(GLVertex2D{
        .position = QVector2D(0.0, screenSize.height()),
        .texcoord = QVector2D(0.0, screenSize.height()),
    });
    verts.push_back(GLVertex2D{
        .position = QVector2D(0.0, screenSize.height()),
        .texcoord = QVector2D(0.0, screenSize.height()),
    });
    verts.push_back(GLVertex2D{
        .position = QVector2D(screenSize.width(), screenSize.height()),
        .texcoord = QVector2D(screenSize.width(), screenSize.height()),
    });
    verts.push_back(GLVertex2D{
        .position = QVector2D(screenSize.width(), 0.0),
        .texcoord = QVector2D(screenSize.width(), 0.0),
    });

    m_vbo->setVertices(verts);
    return true;
}

void LookingGlassEffect::slotWindowAdded(EffectWindow* w)
{
    connect(w, &EffectWindow::windowDamaged, this, &LookingGlassEffect::slotWindowDamaged);
}

void LookingGlassEffect::toggle()
{
    if (target_zoom == 1.0f) {
        target_zoom = 2.0f;
        if (!polling) {
            polling = true;
            effects->startMousePolling();
        }
        m_enabled = true;
    } else {
        target_zoom = 1.0f;
        if (polling) {
            polling = false;
            effects->stopMousePolling();
        }
        if (zoom == target_zoom) {
            m_enabled = false;
        }
    }
    effects->addRepaint(cursorPos().x() - radius, cursorPos().y() - radius, 2 * radius, 2 * radius);
}

void LookingGlassEffect::zoomIn()
{
    target_zoom = qMin(7.0, target_zoom + 0.5);
    m_enabled = true;
    if (!polling) {
        polling = true;
        effects->startMousePolling();
    }
    effects->addRepaint(magnifierArea());
}

void LookingGlassEffect::zoomOut()
{
    target_zoom -= 0.5;
    if (target_zoom < 1) {
        target_zoom = 1;
        if (polling) {
            polling = false;
            effects->stopMousePolling();
        }
        if (zoom == target_zoom) {
            m_enabled = false;
        }
    }
    effects->addRepaint(magnifierArea());
}

QRect LookingGlassEffect::magnifierArea() const
{
    return QRect(cursorPos().x() - radius, cursorPos().y() - radius, 2 * radius, 2 * radius);
}

void LookingGlassEffect::prePaintScreen(effect::screen_prepaint_data& data)
{
    const int time
        = m_lastPresentTime.count() ? (data.present_time - m_lastPresentTime).count() : 0;
    if (zoom != target_zoom) {
        double diff = time / animationTime(500.0);
        if (target_zoom > zoom)
            zoom = qMin(zoom * qMax(1.0 + diff, 1.2), target_zoom);
        else
            zoom = qMax(zoom * qMin(1.0 - diff, 0.8), target_zoom);
        qCDebug(KWIN_LOOKINGGLASS) << "zoom is now " << zoom;
        radius
            = qBound(static_cast<double>(initialradius), initialradius * zoom, 3.5 * initialradius);

        if (zoom <= 1.0f) {
            m_enabled = false;
        }

        effects->addRepaint(
            cursorPos().x() - radius, cursorPos().y() - radius, 2 * radius, 2 * radius);
    }
    if (zoom != target_zoom) {
        m_lastPresentTime = data.present_time;
    } else {
        m_lastPresentTime = std::chrono::milliseconds::zero();
    }
    if (m_valid && m_enabled) {
        data.paint.mask |= PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS;
        // Start rendering to texture
        render::push_framebuffer(data.render, m_fbo.get());
    }

    effects->prePaintScreen(data);
}

void LookingGlassEffect::slotMouseChanged(const QPoint& pos,
                                          const QPoint& old,
                                          Qt::MouseButtons,
                                          Qt::MouseButtons,
                                          Qt::KeyboardModifiers,
                                          Qt::KeyboardModifiers)
{
    if (pos != old && m_enabled) {
        effects->addRepaint(pos.x() - radius, pos.y() - radius, 2 * radius, 2 * radius);
        effects->addRepaint(old.x() - radius, old.y() - radius, 2 * radius, 2 * radius);
    }
}

void LookingGlassEffect::slotWindowDamaged()
{
    if (isActive()) {
        effects->addRepaint(magnifierArea());
    }
}

void LookingGlassEffect::paintScreen(effect::screen_paint_data& data)
{
    // Call the next effect.
    effects->paintScreen(data);
    if (m_valid && m_enabled) {
        // Disable render texture
        auto target = render::pop_framebuffer(data.render);
        Q_ASSERT(target == m_fbo.get());
        Q_UNUSED(target);
        m_texture->bind();
        m_texture->generateMipmaps();

        // Use the shader
        ShaderBinder binder(m_shader.get());
        m_shader->setUniform("u_zoom", static_cast<float>(zoom));
        m_shader->setUniform("u_radius", static_cast<float>(radius));
        m_shader->setUniform("u_cursor", QVector2D(cursorPos().x(), cursorPos().y()));
        m_shader->setUniform(GLShader::ModelViewProjectionMatrix, effect::get_mvp(data));
        m_vbo->render(GL_TRIANGLES);
        m_texture->unbind();
    }
}

bool LookingGlassEffect::isActive() const
{
    return m_valid && m_enabled;
}

} // namespace
