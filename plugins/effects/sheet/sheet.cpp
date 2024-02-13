/*
SPDX-FileCopyrightText: 2007 Philip Falkner <philip.falkner@gmail.com>
SPDX-FileCopyrightText: 2009 Martin Gräßlin <mgraesslin@kde.org>
SPDX-FileCopyrightText: 2018 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "sheet.h"

// KConfigSkeleton
#include "sheetconfig.h"

#include <render/effect/interface/effect_window.h>
#include <render/effect/interface/effects_handler.h>
#include <render/effect/interface/paint_data.h>

#include <QMatrix4x4>

namespace como
{

SheetEffect::SheetEffect()
{
    SheetConfig::instance(effects->config());
    reconfigure(ReconfigureAll);

    connect(effects, &EffectsHandler::windowAdded, this, &SheetEffect::slotWindowAdded);
    connect(effects, &EffectsHandler::windowClosed, this, &SheetEffect::slotWindowClosed);
    connect(effects, &EffectsHandler::windowDeleted, this, &SheetEffect::slotWindowDeleted);
}

void SheetEffect::reconfigure(ReconfigureFlags flags)
{
    Q_UNUSED(flags)

    SheetConfig::self()->read();

    // TODO: Rename AnimationTime config key to Duration.
    const int d
        = animationTime(SheetConfig::animationTime() != 0 ? SheetConfig::animationTime() : 300);
    m_duration = std::chrono::milliseconds(static_cast<int>(d));
}

void SheetEffect::prePaintScreen(effect::screen_prepaint_data& data)
{
    data.paint.mask |= PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS;
    effects->prePaintScreen(data);
}

void SheetEffect::prePaintWindow(effect::window_prepaint_data& data)
{
    if (auto anim_it = m_animations.find(&data.window); anim_it != m_animations.end()) {
        anim_it->timeLine.advance(data.present_time);
        data.paint.mask |= Effect::PAINT_WINDOW_TRANSFORMED;
    }

    effects->prePaintWindow(data);
}

void SheetEffect::paintWindow(effect::window_paint_data& data)
{
    auto animationIt = m_animations.constFind(&data.window);
    if (animationIt == m_animations.constEnd()) {
        effects->paintWindow(data);
        return;
    }

    const qreal t = (*animationIt).timeLine.value();
    data.paint.geo.rotation.axis = {1, 0, 0};
    data.paint.geo.rotation.angle = interpolate(60.0, 0.0, t);
    data.paint.geo.scale *= QVector3D(1.0, t, t);
    data.paint.geo.translation
        += QVector3D(0.0, -interpolate(data.window.y() - (*animationIt).parentY, 0.0, t), 0);

    data.paint.opacity *= t;

    effects->paintWindow(data);
}

void SheetEffect::postPaintWindow(EffectWindow* w)
{
    auto animationIt = m_animations.begin();
    while (animationIt != m_animations.end()) {
        EffectWindow* w = animationIt.key();
        w->addRepaintFull();
        if ((*animationIt).timeLine.done()) {
            animationIt = m_animations.erase(animationIt);
        } else {
            ++animationIt;
        }
    }

    if (m_animations.isEmpty()) {
        effects->addRepaintFull();
    }

    effects->postPaintWindow(w);
}

bool SheetEffect::isActive() const
{
    return !m_animations.isEmpty();
}

bool SheetEffect::supported()
{
    return effects->isOpenGLCompositing() && effects->animationsSupported();
}

void SheetEffect::slotWindowAdded(EffectWindow* w)
{
    if (effects->activeFullScreenEffect()) {
        return;
    }

    if (!isSheetWindow(w)) {
        return;
    }

    Animation& animation = m_animations[w];
    animation.parentY = 0;
    animation.timeLine.reset();
    animation.timeLine.setDuration(m_duration);
    animation.timeLine.setDirection(TimeLine::Forward);
    animation.timeLine.setEasingCurve(QEasingCurve::Linear);

    const auto windows = effects->stackingOrder();
    auto parentIt = std::find_if(windows.constBegin(), windows.constEnd(), [w](EffectWindow* p) {
        return p->findModal() == w;
    });
    if (parentIt != windows.constEnd()) {
        animation.parentY = (*parentIt)->y();
    }

    w->setData(WindowAddedGrabRole, QVariant::fromValue(static_cast<void*>(this)));

    w->addRepaintFull();
}

void SheetEffect::slotWindowClosed(EffectWindow* w)
{
    if (effects->activeFullScreenEffect()) {
        return;
    }

    if (!isSheetWindow(w) || w->skipsCloseAnimation()) {
        return;
    }

    Animation& animation = m_animations[w];

    animation.deletedRef = EffectWindowDeletedRef(w);
    animation.visibleRef = EffectWindowVisibleRef(w, EffectWindow::PAINT_DISABLED_BY_DELETE);
    animation.timeLine.reset();
    animation.parentY = 0;
    animation.timeLine.setDuration(m_duration);
    animation.timeLine.setDirection(TimeLine::Backward);
    animation.timeLine.setEasingCurve(QEasingCurve::Linear);

    const auto windows = effects->stackingOrder();
    auto parentIt = std::find_if(windows.constBegin(), windows.constEnd(), [w](EffectWindow* p) {
        return p->findModal() == w;
    });
    if (parentIt != windows.constEnd()) {
        animation.parentY = (*parentIt)->y();
    }

    w->setData(WindowClosedGrabRole, QVariant::fromValue(static_cast<void*>(this)));

    w->addRepaintFull();
}

void SheetEffect::slotWindowDeleted(EffectWindow* w)
{
    m_animations.remove(w);
}

bool SheetEffect::isSheetWindow(EffectWindow* w) const
{
    return w->isModal();
}

}
