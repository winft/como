/*
SPDX-FileCopyrightText: 2011 Thomas Lübking <thomas.luebking@web.de>
SPDX-FileCopyrightText: 2018 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "anidata_p.h"

#include "effect_window.h"
#include "effects_handler.h"

QDebug operator<<(QDebug dbg, const como::AniData& a)
{
    dbg.nospace() << a.debugInfo();
    return dbg.space();
}

using namespace como;

FullScreenEffectLock::FullScreenEffectLock(Effect* effect)
{
    effects->setActiveFullScreenEffect(effect);
}

FullScreenEffectLock::~FullScreenEffectLock()
{
    effects->setActiveFullScreenEffect(nullptr);
}

PreviousWindowPixmapLock::PreviousWindowPixmapLock(EffectWindow* w)
    : m_window(w)
{
    m_window->referencePreviousWindowPixmap();
}

PreviousWindowPixmapLock::~PreviousWindowPixmapLock()
{
    m_window->unreferencePreviousWindowPixmap();

    // Add synthetic repaint to prevent glitches after cross-fading
    // translucent windows.
    effects->addRepaint(m_window->expandedGeometry());
}

AniData::AniData()
    : attribute(AnimationEffect::Opacity)
    , customCurve(0) // Linear
    , meta(0)
    , startTime(0)
    , frozenTime(-1)
    , waitAtSource(false)
    , keepAlive(true)
{
}

AniData::AniData(AnimationEffect::Attribute a,
                 int meta_,
                 const FPx2& to_,
                 int delay,
                 const FPx2& from_,
                 bool waitAtSource_,
                 FullScreenEffectLockPtr fullScreenEffectLock_,
                 bool keepAlive,
                 PreviousWindowPixmapLockPtr previousWindowPixmapLock_,
                 GLShader* shader)
    : attribute(a)
    , from(from_)
    , to(to_)
    , meta(meta_)
    , startTime(AnimationEffect::clock() + delay)
    , frozenTime(-1)
    , fullScreenEffectLock(std::move(fullScreenEffectLock_))
    , waitAtSource(waitAtSource_)
    , keepAlive(keepAlive)
    , previousWindowPixmapLock(std::move(previousWindowPixmapLock_))
    , shader(shader)
{
}

bool AniData::isActive() const
{
    if (!timeLine.done()) {
        return true;
    }

    if (timeLine.direction() == TimeLine::Backward) {
        return !(terminationFlags & AnimationEffect::TerminateAtSource);
    }

    return !(terminationFlags & AnimationEffect::TerminateAtTarget);
}

static QString attributeString(como::AnimationEffect::Attribute attribute)
{
    switch (attribute) {
    case como::AnimationEffect::Opacity:
        return QStringLiteral("Opacity");
    case como::AnimationEffect::Brightness:
        return QStringLiteral("Brightness");
    case como::AnimationEffect::Saturation:
        return QStringLiteral("Saturation");
    case como::AnimationEffect::Scale:
        return QStringLiteral("Scale");
    case como::AnimationEffect::Translation:
        return QStringLiteral("Translation");
    case como::AnimationEffect::Rotation:
        return QStringLiteral("Rotation");
    case como::AnimationEffect::Position:
        return QStringLiteral("Position");
    case como::AnimationEffect::Size:
        return QStringLiteral("Size");
    case como::AnimationEffect::Clip:
        return QStringLiteral("Clip");
    default:
        return QStringLiteral(" ");
    }
}

QString AniData::debugInfo() const
{
    return QLatin1String("Animation: ") + attributeString(attribute)
        + QLatin1String("\n     From: ") + from.toString() + QLatin1String("\n       To: ")
        + to.toString() + QLatin1String("\n  Started: ")
        + QString::number(AnimationEffect::clock() - startTime) + QLatin1String("ms ago\n")
        + QLatin1String(" Duration: ") + QString::number(timeLine.duration().count())
        + QLatin1String("ms\n") + QLatin1String("   Passed: ")
        + QString::number(timeLine.elapsed().count()) + QLatin1String("ms\n");
}
