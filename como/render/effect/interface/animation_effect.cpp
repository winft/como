/*
    SPDX-FileCopyrightText: 2011 Thomas Lübking <thomas.luebking@web.de>
    SPDX-FileCopyrightText: 2018 Vlad Zahorodnii <vlad.zahorodnii@kde.org>
    SPDX-FileCopyrightText: 2022 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "animation_effect.h"

#include "anidata_p.h"
#include "effect_window.h"
#include "effects_handler.h"
#include "paint_data.h"
#include <como/render/gl/interface/shader.h>
#include <como/render/gl/interface/shader_manager.h>

#include <QDebug>
#include <QTimer>

QDebug operator<<(QDebug dbg, const como::FPx2& fpx2)
{
    dbg.nospace() << fpx2[0] << "," << fpx2[1]
                  << QString(fpx2.isValid() ? QStringLiteral(" (valid)")
                                            : QStringLiteral(" (invalid)"));
    return dbg.space();
}

namespace como
{

QElapsedTimer AnimationEffect::s_clock;

class AnimationEffectPrivate
{
public:
    AnimationEffectPrivate()
    {
        m_animationsTouched = m_isInitialized = false;
        m_justEndedAnimation = 0;
    }
    AnimationEffect::AniMap m_animations;
    static quint64 m_animCounter;
    quint64 m_justEndedAnimation; // protect against cancel
    QWeakPointer<FullScreenEffectLock> m_fullScreenEffectLock;
    bool m_needSceneRepaint, m_animationsTouched, m_isInitialized;
};
}

using namespace como;

quint64 AnimationEffectPrivate::m_animCounter = 0;

AnimationEffect::AnimationEffect()
    : d_ptr{std::make_unique<AnimationEffectPrivate>()}
{
    if (!s_clock.isValid())
        s_clock.start();
    /* this is the same as the QTimer::singleShot(0, SLOT(init())) kludge
     * defering the init and esp. the connection to the windowClosed slot */
    QMetaObject::invokeMethod(this, "init", Qt::QueuedConnection);
}

AnimationEffect::~AnimationEffect() = default;

void AnimationEffect::init()
{
    if (d_ptr->m_isInitialized)
        return; // not more than once, please
    d_ptr->m_isInitialized = true;
    /* by connecting the signal from a slot AFTER the inheriting class constructor had the chance to
     * connect it we can provide auto-referencing of animated and closed windows, since at the time
     * our slot will be called, the slot of the subclass has been (SIGNAL/SLOT connections are FIFO)
     * and has pot. started an animation so we have the window in our hash :) */
    connect(effects, &EffectsHandler::windowClosed, this, &AnimationEffect::_windowClosed);
    connect(effects, &EffectsHandler::windowDeleted, this, &AnimationEffect::_windowDeleted);
}

bool AnimationEffect::isActive() const
{
    return !d_ptr->m_animations.isEmpty() && !effects->isScreenLocked();
}

#define RELATIVE_XY(_FIELD_)                                                                       \
    const bool relative[2] = {static_cast<bool>(metaData(Relative##_FIELD_##X, meta)),             \
                              static_cast<bool>(metaData(Relative##_FIELD_##Y, meta))}

void AnimationEffect::validate(Attribute a,
                               uint& meta,
                               FPx2* from,
                               FPx2* to,
                               const EffectWindow* w) const
{
    if (a < NonFloatBase) {
        if (a == Scale) {
            QRect area = effects->clientArea(ScreenArea, w);
            if (from && from->isValid()) {
                RELATIVE_XY(Source);
                from->set(relative[0] ? (*from)[0] * area.width() / w->width() : (*from)[0],
                          relative[1] ? (*from)[1] * area.height() / w->height() : (*from)[1]);
            }
            if (to && to->isValid()) {
                RELATIVE_XY(Target);
                to->set(relative[0] ? (*to)[0] * area.width() / w->width() : (*to)[0],
                        relative[1] ? (*to)[1] * area.height() / w->height() : (*to)[1]);
            }
        } else if (a == Rotation) {
            if (from && !from->isValid()) {
                setMetaData(SourceAnchor, metaData(TargetAnchor, meta), meta);
                from->set(0.0, 0.0);
            }
            if (to && !to->isValid()) {
                setMetaData(TargetAnchor, metaData(SourceAnchor, meta), meta);
                to->set(0.0, 0.0);
            }
        }
        if (from && !from->isValid())
            from->set(1.0, 1.0);
        if (to && !to->isValid())
            to->set(1.0, 1.0);

    } else if (a == Position) {
        QRect area = effects->clientArea(ScreenArea, w);
        QPoint pt = w->frameGeometry().bottomRight(); // cannot be < 0 ;-)
        if (from) {
            if (from->isValid()) {
                RELATIVE_XY(Source);
                from->set(relative[0] ? area.x() + (*from)[0] * area.width() : (*from)[0],
                          relative[1] ? area.y() + (*from)[1] * area.height() : (*from)[1]);
            } else {
                from->set(pt.x(), pt.y());
                setMetaData(SourceAnchor, AnimationEffect::Bottom | AnimationEffect::Right, meta);
            }
        }

        if (to) {
            if (to->isValid()) {
                RELATIVE_XY(Target);
                to->set(relative[0] ? area.x() + (*to)[0] * area.width() : (*to)[0],
                        relative[1] ? area.y() + (*to)[1] * area.height() : (*to)[1]);
            } else {
                to->set(pt.x(), pt.y());
                setMetaData(TargetAnchor, AnimationEffect::Bottom | AnimationEffect::Right, meta);
            }
        }

    } else if (a == Size) {
        QRect area = effects->clientArea(ScreenArea, w);
        if (from) {
            if (from->isValid()) {
                RELATIVE_XY(Source);
                from->set(relative[0] ? (*from)[0] * area.width() : (*from)[0],
                          relative[1] ? (*from)[1] * area.height() : (*from)[1]);
            } else {
                from->set(w->width(), w->height());
            }
        }

        if (to) {
            if (to->isValid()) {
                RELATIVE_XY(Target);
                to->set(relative[0] ? (*to)[0] * area.width() : (*to)[0],
                        relative[1] ? (*to)[1] * area.height() : (*to)[1]);
            } else {
                to->set(w->width(), w->height());
            }
        }

    } else if (a == Translation) {
        QRect area = w->rect();
        if (from) {
            if (from->isValid()) {
                RELATIVE_XY(Source);
                from->set(relative[0] ? (*from)[0] * area.width() : (*from)[0],
                          relative[1] ? (*from)[1] * area.height() : (*from)[1]);
            } else {
                from->set(0.0, 0.0);
            }
        }

        if (to) {
            if (to->isValid()) {
                RELATIVE_XY(Target);
                to->set(relative[0] ? (*to)[0] * area.width() : (*to)[0],
                        relative[1] ? (*to)[1] * area.height() : (*to)[1]);
            } else {
                to->set(0.0, 0.0);
            }
        }

    } else if (a == Clip) {
        if (from && !from->isValid()) {
            from->set(1.0, 1.0);
            setMetaData(SourceAnchor, metaData(TargetAnchor, meta), meta);
        }
        if (to && !to->isValid()) {
            to->set(1.0, 1.0);
            setMetaData(TargetAnchor, metaData(SourceAnchor, meta), meta);
        }

    } else if (a == CrossFadePrevious) {
        if (from && !from->isValid()) {
            from->set(0.0);
        }
        if (to && !to->isValid()) {
            to->set(1.0);
        }
    }
}

quint64 AnimationEffect::p_animate(EffectWindow* w,
                                   Attribute a,
                                   uint meta,
                                   int ms,
                                   FPx2 to,
                                   const QEasingCurve& curve,
                                   int delay,
                                   FPx2 from,
                                   bool keepAtTarget,
                                   bool fullScreenEffect,
                                   bool keepAlive,
                                   GLShader* shader)
{
    const bool waitAtSource = from.isValid();
    validate(a, meta, &from, &to, w);

    if (!d_ptr->m_isInitialized)
        init(); // needs to ensure the window gets removed if deleted in the same event cycle
    auto it = d_ptr->m_animations.find(w);
    if (it == d_ptr->m_animations.end()) {
        connect(w,
                &EffectWindow::windowExpandedGeometryChanged,
                this,
                &AnimationEffect::_windowExpandedGeometryChanged);
        it = d_ptr->m_animations.insert(w, QPair<QList<AniData>, QRect>(QList<AniData>(), QRect()));
    }

    FullScreenEffectLockPtr fullscreen;
    if (fullScreenEffect) {
        if (d_ptr->m_fullScreenEffectLock.isNull()) {
            fullscreen = FullScreenEffectLockPtr::create(this);
            d_ptr->m_fullScreenEffectLock = fullscreen.toWeakRef();
        } else {
            fullscreen = d_ptr->m_fullScreenEffectLock.toStrongRef();
        }
    }

    PreviousWindowPixmapLockPtr previousPixmap;
    if (a == CrossFadePrevious) {
        previousPixmap = PreviousWindowPixmapLockPtr::create(w);
    }

    it->first.append(AniData(a,              // Attribute
                             meta,           // Metadata
                             to,             // Target
                             delay,          // Delay
                             from,           // Source
                             waitAtSource,   // Whether the animation should be kept at source
                             fullscreen,     // Full screen effect lock
                             keepAlive,      // Keep alive flag
                             previousPixmap, // Previous window pixmap lock
                             shader));

    const quint64 ret_id = ++d_ptr->m_animCounter;
    AniData& animation = it->first.last();
    animation.id = ret_id;

    animation.visibleRef = EffectWindowVisibleRef(w,
                                                  EffectWindow::PAINT_DISABLED_BY_MINIMIZE
                                                      | EffectWindow::PAINT_DISABLED_BY_DESKTOP
                                                      | EffectWindow::PAINT_DISABLED_BY_DELETE);
    animation.timeLine.setDirection(TimeLine::Forward);
    animation.timeLine.setDuration(std::chrono::milliseconds(ms));
    animation.timeLine.setEasingCurve(curve);
    animation.timeLine.setSourceRedirectMode(TimeLine::RedirectMode::Strict);
    animation.timeLine.setTargetRedirectMode(TimeLine::RedirectMode::Relaxed);

    animation.terminationFlags = TerminateAtSource;
    if (!keepAtTarget) {
        animation.terminationFlags |= TerminateAtTarget;
    }

    it->second = QRect();

    d_ptr->m_animationsTouched = true;

    if (delay > 0) {
        QTimer::singleShot(delay, this, &AnimationEffect::triggerRepaint);
        const QSize& s = effects->virtualScreenSize();
        if (waitAtSource) {
            w->addLayerRepaint(0, 0, s.width(), s.height());
        }
    } else {
        triggerRepaint();
    }

    if (shader) {
        OffscreenEffect::redirect(w);
    }

    return ret_id;
}

bool AnimationEffect::retarget(quint64 animationId, FPx2 newTarget, int newRemainingTime)
{
    if (animationId == d_ptr->m_justEndedAnimation) {
        // this is just ending, do not try to retarget it
        return false;
    }

    for (auto entry = d_ptr->m_animations.begin(), mapEnd = d_ptr->m_animations.end();
         entry != mapEnd;
         ++entry) {
        for (auto anim = entry->first.begin(), animEnd = entry->first.end(); anim != animEnd;
             ++anim) {
            if (anim->id == animationId) {
                anim->from.set(interpolated(*anim, 0), interpolated(*anim, 1));
                validate(anim->attribute, anim->meta, nullptr, &newTarget, entry.key());
                anim->to.set(newTarget[0], newTarget[1]);

                anim->timeLine.setDirection(TimeLine::Forward);
                anim->timeLine.setDuration(std::chrono::milliseconds(newRemainingTime));
                anim->timeLine.reset();

                return true;
            }
        }
    }
    return false; // no animation found
}

bool AnimationEffect::freezeInTime(quint64 animationId, qint64 frozenTime)
{
    if (animationId == d_ptr->m_justEndedAnimation) {
        return false; // this is just ending, do not try to retarget it
    }
    for (auto entry = d_ptr->m_animations.begin(), mapEnd = d_ptr->m_animations.end();
         entry != mapEnd;
         ++entry) {
        for (auto anim = entry->first.begin(), animEnd = entry->first.end(); anim != animEnd;
             ++anim) {
            if (anim->id == animationId) {
                if (frozenTime >= 0) {
                    anim->timeLine.setElapsed(std::chrono::milliseconds(frozenTime));
                }
                anim->frozenTime = frozenTime;
                return true;
            }
        }
    }
    return false; // no animation found
}

bool AnimationEffect::redirect(quint64 animationId,
                               Direction direction,
                               TerminationFlags terminationFlags)
{
    if (animationId == d_ptr->m_justEndedAnimation) {
        return false;
    }

    for (auto entryIt = d_ptr->m_animations.begin(); entryIt != d_ptr->m_animations.end();
         ++entryIt) {
        auto animIt = std::find_if(entryIt->first.begin(),
                                   entryIt->first.end(),
                                   [animationId](AniData& anim) { return anim.id == animationId; });
        if (animIt == entryIt->first.end()) {
            continue;
        }

        switch (direction) {
        case Backward:
            animIt->timeLine.setDirection(TimeLine::Backward);
            break;

        case Forward:
            animIt->timeLine.setDirection(TimeLine::Forward);
            break;
        }

        animIt->terminationFlags = terminationFlags & ~TerminateAtTarget;

        return true;
    }

    return false;
}

bool AnimationEffect::complete(quint64 animationId)
{
    if (animationId == d_ptr->m_justEndedAnimation) {
        return false;
    }

    for (auto entryIt = d_ptr->m_animations.begin(); entryIt != d_ptr->m_animations.end();
         ++entryIt) {
        auto animIt = std::find_if(entryIt->first.begin(),
                                   entryIt->first.end(),
                                   [animationId](AniData& anim) { return anim.id == animationId; });
        if (animIt == entryIt->first.end()) {
            continue;
        }

        animIt->timeLine.setElapsed(animIt->timeLine.duration());

        return true;
    }

    return false;
}

bool AnimationEffect::cancel(quint64 animationId)
{
    if (animationId == d_ptr->m_justEndedAnimation) {
        // this is just ending, do not try to cancel it but fake success
        return true;
    }

    for (auto entry = d_ptr->m_animations.begin(), mapEnd = d_ptr->m_animations.end();
         entry != mapEnd;
         ++entry) {
        for (auto anim = entry->first.begin(), animEnd = entry->first.end(); anim != animEnd;
             ++anim) {
            if (anim->id == animationId) {
                if (anim->shader
                    && std::none_of(
                        entry->first.begin(), entry->first.end(), [animationId](const auto& anim) {
                            return anim.id != animationId && anim.shader;
                        })) {
                    unredirect(entry.key());
                }
                entry->first.erase(anim);     // remove the animation
                if (entry->first.isEmpty()) { // no other animations on the window, release it.
                    disconnect(entry.key(),
                               &EffectWindow::windowExpandedGeometryChanged,
                               this,
                               &AnimationEffect::_windowExpandedGeometryChanged);
                    d_ptr->m_animations.erase(entry);
                }
                d_ptr->m_animationsTouched = true; // could be called from animationEnded
                return true;
            }
        }
    }
    return false;
}

static int xCoord(const QRect& r, int flag)
{
    if (flag & AnimationEffect::Left) {
        return r.x();
    }
    if (flag & AnimationEffect::Right) {
        return r.right();
    }
    return r.x() + r.width() / 2;
}

static int yCoord(const QRect& r, int flag)
{
    if (flag & AnimationEffect::Top) {
        return r.y();
    }
    if (flag & AnimationEffect::Bottom) {
        return r.bottom();
    }
    return r.y() + r.height() / 2;
}

QRect AnimationEffect::clipRect(const QRect& geo, const AniData& anim) const
{
    QRect clip = geo;
    FPx2 ratio = anim.from + progress(anim) * (anim.to - anim.from);
    if (anim.from[0] < 1.0 || anim.to[0] < 1.0) {
        clip.setWidth(clip.width() * ratio[0]);
    }
    if (anim.from[1] < 1.0 || anim.to[1] < 1.0) {
        clip.setHeight(clip.height() * ratio[1]);
    }
    auto const center = geo.adjusted(
        clip.width() / 2, clip.height() / 2, -(clip.width() + 1) / 2, -(clip.height() + 1) / 2);
    const int x[2] = {xCoord(center, metaData(SourceAnchor, anim.meta)),
                      xCoord(center, metaData(TargetAnchor, anim.meta))};
    const int y[2] = {yCoord(center, metaData(SourceAnchor, anim.meta)),
                      yCoord(center, metaData(TargetAnchor, anim.meta))};
    const QPoint d(x[0] + ratio[0] * (x[1] - x[0]), y[0] + ratio[1] * (y[1] - y[0]));
    clip.moveTopLeft(QPoint(d.x() - clip.width() / 2, d.y() - clip.height() / 2));
    return clip;
}

void AnimationEffect::prePaintWindow(effect::window_prepaint_data& data)
{
    auto entry = d_ptr->m_animations.find(&data.window);

    if (entry != d_ptr->m_animations.end()) {
        for (auto anim = entry->first.begin(); anim != entry->first.end(); ++anim) {
            if (anim->startTime > clock() && !anim->waitAtSource) {
                continue;
            }

            if (anim->frozenTime < 0) {
                anim->timeLine.advance(data.present_time);
            }

            if (anim->attribute == Opacity || anim->attribute == CrossFadePrevious) {
                data.set_translucent();
            } else if (!(anim->attribute == Brightness || anim->attribute == Saturation)) {
                data.paint.mask |= Effect::PAINT_WINDOW_TRANSFORMED;
            }
        }
    }

    effects->prePaintWindow(data);
}

static inline float geometryCompensation(int flags, float v)
{
    if (flags & (AnimationEffect::Left | AnimationEffect::Top))
        return 0.0; // no compensation required
    if (flags & (AnimationEffect::Right | AnimationEffect::Bottom))
        return 1.0 - v;     // full compensation
    return 0.5 * (1.0 - v); // half compensation
}

void AnimationEffect::paintWindow(effect::window_paint_data& data)
{
    auto entry = d_ptr->m_animations.constFind(&data.window);
    if (entry != d_ptr->m_animations.constEnd()) {
        for (QList<AniData>::const_iterator anim = entry->first.constBegin();
             anim != entry->first.constEnd();
             ++anim) {

            if (anim->startTime > clock() && !anim->waitAtSource)
                continue;

            switch (anim->attribute) {
            case Opacity:
                data.paint.opacity *= interpolated(*anim);
                break;
            case Brightness:
                data.paint.brightness *= interpolated(*anim);
                break;
            case Saturation:
                data.paint.saturation *= interpolated(*anim);
                break;
            case Scale: {
                auto const sz = data.window.frameGeometry().size();
                float f1(1.0), f2(0.0);
                if (anim->from[0] >= 0.0 && anim->to[0] >= 0.0) { // scale x
                    f1 = interpolated(*anim, 0);
                    f2 = geometryCompensation(anim->meta & AnimationEffect::Horizontal, f1);
                    data.paint.geo.translation += QVector3D(f2 * sz.width(), 0, 0);
                    data.paint.geo.scale *= QVector3D(f1, 1, 1);
                }
                if (anim->from[1] >= 0.0 && anim->to[1] >= 0.0) { // scale y
                    if (!anim->isOneDimensional()) {
                        f1 = interpolated(*anim, 1);
                        f2 = geometryCompensation(anim->meta & AnimationEffect::Vertical, f1);
                    } else if (((anim->meta & AnimationEffect::Vertical) >> 1)
                               != (anim->meta & AnimationEffect::Horizontal))
                        f2 = geometryCompensation(anim->meta & AnimationEffect::Vertical, f1);
                    data.paint.geo.translation += QVector3D(0, f2 * sz.height(), 0);
                    data.paint.geo.scale *= QVector3D(1, f1, 1);
                }
                break;
            }
            case Clip:
                data.paint.region = clipRect(data.window.expandedGeometry(), *anim);
                break;
            case Translation:
                data.paint.geo.translation
                    += QVector3D(interpolated(*anim, 0), interpolated(*anim, 1), 0);
                break;
            case Size: {
                FPx2 dest = anim->from + progress(*anim) * (anim->to - anim->from);
                auto const sz = data.window.frameGeometry().size();
                float f;
                if (anim->from[0] >= 0.0 && anim->to[0] >= 0.0) { // resize x
                    f = dest[0] / sz.width();
                    data.paint.geo.translation += QVector3D(
                        geometryCompensation(anim->meta & AnimationEffect::Horizontal, f)
                            * sz.width(),
                        0,
                        0);
                    data.paint.geo.scale *= QVector3D(f, 1, 1);
                }
                if (anim->from[1] >= 0.0 && anim->to[1] >= 0.0) { // resize y
                    f = dest[1] / sz.height();
                    data.paint.geo.translation
                        += QVector3D(0.0,
                                     geometryCompensation(anim->meta & AnimationEffect::Vertical, f)
                                         * sz.height(),
                                     0);
                    data.paint.geo.scale *= QVector3D(1, f, 1);
                }
                break;
            }
            case Position: {
                auto const geo = data.window.frameGeometry();
                const float prgrs = progress(*anim);
                if (anim->from[0] >= 0.0 && anim->to[0] >= 0.0) {
                    float dest = interpolated(*anim, 0);
                    int const x[2] = {xCoord(geo, metaData(SourceAnchor, anim->meta)),
                                      xCoord(geo, metaData(TargetAnchor, anim->meta))};
                    data.paint.geo.translation
                        += QVector3D(dest - (x[0] + prgrs * (x[1] - x[0])), 0, 0);
                }
                if (anim->from[1] >= 0.0 && anim->to[1] >= 0.0) {
                    float dest = interpolated(*anim, 1);
                    const int y[2] = {yCoord(geo, metaData(SourceAnchor, anim->meta)),
                                      yCoord(geo, metaData(TargetAnchor, anim->meta))};
                    data.paint.geo.translation
                        += QVector3D(0.0, dest - (y[0] + prgrs * (y[1] - y[0])), 0);
                }
                break;
            }
            case Rotation: {
                auto& rot = data.paint.geo.rotation;

                auto const prgrs = progress(*anim);
                rot.angle = anim->from[0] + prgrs * (anim->to[0] - anim->from[0]);

                auto const axis_meta = static_cast<Qt::Axis>(metaData(Axis, anim->meta));
                if (axis_meta == Qt::XAxis) {
                    rot.axis = {1, 0, 0};
                } else if (axis_meta == Qt::YAxis) {
                    rot.axis = {0, 1, 0};
                } else if (axis_meta == Qt::ZAxis) {
                    rot.axis = {0, 0, 1};
                }

                auto const geo = data.window.rect();
                uint const sAnchor = metaData(SourceAnchor, anim->meta),
                           tAnchor = metaData(TargetAnchor, anim->meta);
                QPointF pt(xCoord(geo, sAnchor), yCoord(geo, sAnchor));

                if (tAnchor != sAnchor) {
                    QPointF pt2(xCoord(geo, tAnchor), yCoord(geo, tAnchor));
                    pt += static_cast<qreal>(prgrs) * (pt2 - pt);
                }

                rot.origin = QVector3D(pt);
                break;
            }
            case Generic:
                genericAnimation(data, progress(*anim), anim->meta);
                break;
            case CrossFadePrevious:
                data.cross_fade_progress = qBound(0., progress(*anim), 1.);
                break;
            case Shader:
                if (anim->shader && anim->shader->isValid()) {
                    ShaderBinder binder{anim->shader};
                    anim->shader->setUniform("animationProgress", progress(*anim));
                    setShader(data.window, anim->shader);
                }
                break;
            case ShaderUniform:
                if (anim->shader && anim->shader->isValid()) {
                    ShaderBinder binder{anim->shader};
                    anim->shader->setUniform("animationProgress", progress(*anim));
                    anim->shader->setUniform(anim->meta, interpolated(*anim));
                    setShader(data.window, anim->shader);
                }
                break;
            default:
                break;
            }
        }
    }

    effects->paintWindow(data);
}

void AnimationEffect::postPaintScreen()
{
    d_ptr->m_animationsTouched = false;
    bool damageDirty = false;

    for (auto entry = d_ptr->m_animations.begin(); entry != d_ptr->m_animations.end();) {
        bool invalidateLayerRect = false;
        int animCounter = 0;
        for (auto anim = entry->first.begin(); anim != entry->first.end();) {
            if (anim->isActive() || (anim->startTime > clock() && !anim->waitAtSource)) {
                ++anim;
                ++animCounter;
                continue;
            }
            auto window = entry.key();
            d_ptr->m_justEndedAnimation = anim->id;
            if (anim->shader
                && std::none_of(
                    entry->first.begin(), entry->first.end(), [anim](const auto& other) {
                        return anim->id != other.id && other.shader;
                    })) {
                unredirect(window);
            }
            animationEnded(window, anim->attribute, anim->meta);
            d_ptr->m_justEndedAnimation = 0;
            // NOTICE animationEnded is an external call and might have called "::animate"
            // as a result our iterators could now point random junk on the heap
            // so we've to restore the former states, ie. find our window list and animation
            if (d_ptr->m_animationsTouched) {
                d_ptr->m_animationsTouched = false;
                entry = d_ptr->m_animations.begin();
                while (entry.key() != window && entry != d_ptr->m_animations.end()) {
                    ++entry;
                }
                Q_ASSERT(
                    entry
                    != d_ptr->m_animations.end()); // usercode should not delete animations from
                                                   // animationEnded (not even possible atm.)
                anim = entry->first.begin();
                Q_ASSERT(animCounter < entry->first.count());
                for (int i = 0; i < animCounter; ++i) {
                    ++anim;
                }
            }
            anim = entry->first.erase(anim);
            invalidateLayerRect = damageDirty = true;
        }
        if (entry->first.isEmpty()) {
            disconnect(entry.key(),
                       &EffectWindow::windowExpandedGeometryChanged,
                       this,
                       &AnimationEffect::_windowExpandedGeometryChanged);
            effects->addRepaint(entry->second);
            entry = d_ptr->m_animations.erase(entry);
        } else {
            if (invalidateLayerRect) {
                *const_cast<QRect*>(&(entry->second)) = QRect(); // invalidate
            }
            ++entry;
        }
    }

    if (damageDirty) {
        updateLayerRepaints();
    }
    if (d_ptr->m_needSceneRepaint) {
        effects->addRepaintFull();
    } else {
        for (auto entry = d_ptr->m_animations.constBegin(); entry != d_ptr->m_animations.constEnd();
             ++entry) {
            for (auto anim = entry->first.constBegin(); anim != entry->first.constEnd(); ++anim) {
                if (anim->startTime > clock())
                    continue;
                if (!anim->timeLine.done()) {
                    entry.key()->addLayerRepaint(entry->second);
                    break;
                }
            }
        }
    }

    effects->postPaintScreen();
}

float AnimationEffect::interpolated(const AniData& a, int i) const
{
    return a.from[i] + a.timeLine.value() * (a.to[i] - a.from[i]);
}

float AnimationEffect::progress(const AniData& a) const
{
    return a.startTime < clock() ? a.timeLine.value() : 0.0;
}

// TODO - get this out of the header - the functionpointer usage of QEasingCurve somehow sucks ;-)
// qreal AnimationEffect::qecGaussian(qreal progress) // exp(-5*(2*x-1)^2)
// {
//     progress = 2*progress - 1;
//     progress *= -5*progress;
//     return qExp(progress);
// }

int AnimationEffect::metaData(MetaType type, uint meta)
{
    switch (type) {
    case SourceAnchor:
        return ((meta >> 5) & 0x1f);
    case TargetAnchor:
        return (meta & 0x1f);
    case RelativeSourceX:
    case RelativeSourceY:
    case RelativeTargetX:
    case RelativeTargetY: {
        const int shift = 10 + type - RelativeSourceX;
        return ((meta >> shift) & 1);
    }
    case Axis:
        return ((meta >> 10) & 3);
    default:
        return 0;
    }
}

void AnimationEffect::setMetaData(MetaType type, uint value, uint& meta)
{
    switch (type) {
    case SourceAnchor:
        meta &= ~(0x1f << 5);
        meta |= ((value & 0x1f) << 5);
        break;
    case TargetAnchor:
        meta &= ~(0x1f);
        meta |= (value & 0x1f);
        break;
    case RelativeSourceX:
    case RelativeSourceY:
    case RelativeTargetX:
    case RelativeTargetY: {
        const int shift = 10 + type - RelativeSourceX;
        if (value)
            meta |= (1 << shift);
        else
            meta &= ~(1 << shift);
        break;
    }
    case Axis:
        meta &= ~(3 << 10);
        meta |= ((value & 3) << 10);
        break;
    default:
        break;
    }
}

void AnimationEffect::triggerRepaint()
{
    for (auto entry = d_ptr->m_animations.constBegin(); entry != d_ptr->m_animations.constEnd();
         ++entry) {
        *const_cast<QRect*>(&(entry->second)) = QRect();
    }
    updateLayerRepaints();
    if (d_ptr->m_needSceneRepaint) {
        effects->addRepaintFull();
    } else {
        for (auto it = d_ptr->m_animations.constBegin(); it != d_ptr->m_animations.constEnd();
             ++it) {
            it.key()->addLayerRepaint(it->second);
        }
    }
}

static float fixOvershoot(float f, const AniData& d, short int dir, float s = 1.1)
{
    switch (d.timeLine.easingCurve().type()) {
    case QEasingCurve::InOutElastic:
    case QEasingCurve::InOutBack:
        return f * s;
    case QEasingCurve::InElastic:
    case QEasingCurve::OutInElastic:
    case QEasingCurve::OutBack:
        return (dir & 2) ? f * s : f;
    case QEasingCurve::OutElastic:
    case QEasingCurve::InBack:
        return (dir & 1) ? f * s : f;
    default:
        return f;
    }
}

void AnimationEffect::updateLayerRepaints()
{
    d_ptr->m_needSceneRepaint = false;
    for (auto entry = d_ptr->m_animations.constBegin(); entry != d_ptr->m_animations.constEnd();
         ++entry) {
        if (!entry->second.isNull()) {
            continue;
        }

        float f[2] = {1.0, 1.0};
        float t[2] = {0.0, 0.0};
        bool createRegion = false;
        QList<QRect> rects;
        QRect* layerRect = const_cast<QRect*>(&(entry->second));

        for (auto anim = entry->first.constBegin(); anim != entry->first.constEnd(); ++anim) {
            if (anim->startTime > clock()) {
                continue;
            }
            switch (anim->attribute) {
            case Opacity:
            case Brightness:
            case Saturation:
            case CrossFadePrevious:
            case Shader:
            case ShaderUniform:
                createRegion = true;
                break;
            case Rotation:
                createRegion = false;
                *layerRect = QRect(QPoint(0, 0), effects->virtualScreenSize());
                goto region_creation; // sic! no need to do anything else
            case Generic:
                // we don't know whether this will change visual stacking order
                // sic! no need to do anything else
                d_ptr->m_needSceneRepaint = true;
                return;
            case Translation:
            case Position: {
                createRegion = true;
                QRect r(entry.key()->frameGeometry());
                int x[2] = {0, 0};
                int y[2] = {0, 0};
                if (anim->attribute == Translation) {
                    x[0] = anim->from[0];
                    x[1] = anim->to[0];
                    y[0] = anim->from[1];
                    y[1] = anim->to[1];
                } else {
                    if (anim->from[0] >= 0.0 && anim->to[0] >= 0.0) {
                        x[0] = anim->from[0] - xCoord(r, metaData(SourceAnchor, anim->meta));
                        x[1] = anim->to[0] - xCoord(r, metaData(TargetAnchor, anim->meta));
                    }
                    if (anim->from[1] >= 0.0 && anim->to[1] >= 0.0) {
                        y[0] = anim->from[1] - yCoord(r, metaData(SourceAnchor, anim->meta));
                        y[1] = anim->to[1] - yCoord(r, metaData(TargetAnchor, anim->meta));
                    }
                }
                r = entry.key()->expandedGeometry();
                rects << r.translated(x[0], y[0]) << r.translated(x[1], y[1]);
                break;
            }
            case Clip:
                createRegion = true;
                break;
            case Size:
            case Scale: {
                createRegion = true;
                const QSize sz = entry.key()->frameGeometry().size();
                float fx = qMax(fixOvershoot(anim->from[0], *anim, 1),
                                fixOvershoot(anim->to[0], *anim, 2));
                //                     float fx = qMax(interpolated(*anim,0), anim->to[0]);
                if (fx >= 0.0) {
                    if (anim->attribute == Size)
                        fx /= sz.width();
                    f[0] *= fx;
                    t[0] += geometryCompensation(anim->meta & AnimationEffect::Horizontal, fx)
                        * sz.width();
                }
                //                     float fy = qMax(interpolated(*anim,1), anim->to[1]);
                float fy = qMax(fixOvershoot(anim->from[1], *anim, 1),
                                fixOvershoot(anim->to[1], *anim, 2));
                if (fy >= 0.0) {
                    if (anim->attribute == Size)
                        fy /= sz.height();
                    if (!anim->isOneDimensional()) {
                        f[1] *= fy;
                        t[1] += geometryCompensation(anim->meta & AnimationEffect::Vertical, fy)
                            * sz.height();
                    } else if (((anim->meta & AnimationEffect::Vertical) >> 1)
                               != (anim->meta & AnimationEffect::Horizontal)) {
                        f[1] *= fx;
                        t[1] += geometryCompensation(anim->meta & AnimationEffect::Vertical, fx)
                            * sz.height();
                    }
                }
                break;
            }
            }
        }
    region_creation:
        if (createRegion) {
            auto const geo = entry.key()->expandedGeometry();
            if (rects.isEmpty()) {
                rects << geo;
            }

            auto r = rects.constEnd();
            auto rEnd = r;

            for (r = rects.constBegin(); r != rEnd; ++r) {
                // transform
                const_cast<QRect*>(&(*r))->setSize(
                    QSize(qRound(r->width() * f[0]), qRound(r->height() * f[1])));
                const_cast<QRect*>(&(*r))->translate(t[0], t[1]);
            }

            auto rect = rects.at(0);
            if (rects.count() > 1) {
                for (r = rects.constBegin() + 1; r != rEnd; ++r) // unite
                    rect |= *r;
                const int dx
                    = 110 * (rect.width() - geo.width()) / 100 + 1 - rect.width() + geo.width();
                const int dy
                    = 110 * (rect.height() - geo.height()) / 100 + 1 - rect.height() + geo.height();
                rect.adjust(-dx, -dy, dx, dy); // fix pot. overshoot
            }
            *layerRect = rect;
        }
    }
}

void AnimationEffect::_windowExpandedGeometryChanged(como::EffectWindow* w)
{
    if (auto entry = d_ptr->m_animations.constFind(w); entry != d_ptr->m_animations.constEnd()) {
        *const_cast<QRect*>(&(entry->second)) = QRect();
        updateLayerRepaints();
        if (!entry->second.isNull()) {
            // actually got updated, ie. is in use - ensure it get's a repaint
            w->addLayerRepaint(entry->second);
        }
    }
}

void AnimationEffect::_windowClosed(EffectWindow* w)
{
    auto it = d_ptr->m_animations.find(w);
    if (it == d_ptr->m_animations.end()) {
        return;
    }

    auto& animations = (*it).first;
    for (auto animationIt = animations.begin(); animationIt != animations.end(); ++animationIt) {
        if (animationIt->keepAlive) {
            animationIt->deletedRef = EffectWindowDeletedRef(w);
        }
    }
}

void AnimationEffect::_windowDeleted(EffectWindow* w)
{
    d_ptr->m_animations.remove(w);
}

QString AnimationEffect::debug(const QString& /*parameter*/) const
{
    if (d_ptr->m_animations.isEmpty()) {
        return QStringLiteral("No window is animated");
    }

    QString dbg;

    for (auto entry = d_ptr->m_animations.constBegin(); entry != d_ptr->m_animations.constEnd();
         ++entry) {
        auto caption
            = entry.key()->isDeleted() ? QStringLiteral("[Deleted]") : entry.key()->caption();
        if (caption.isEmpty()) {
            caption = QStringLiteral("[Untitled]");
        }
        dbg += QLatin1String("Animating window: ") + caption + QLatin1Char('\n');

        for (auto anim = entry->first.constBegin(); anim != entry->first.constEnd(); ++anim)
            dbg += anim->debugInfo();
    }

    return dbg;
}

AnimationEffect::AniMap AnimationEffect::state() const
{
    return d_ptr->m_animations;
}
