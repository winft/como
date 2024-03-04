/*
SPDX-FileCopyrightText: 2011 Thomas Lübking <thomas.luebking@web.de>
SPDX-FileCopyrightText: 2018 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef ANIDATA_H
#define ANIDATA_H

#include "animation_effect.h"
#include "effect_window_deleted_ref.h"
#include "effect_window_visible_ref.h"
#include "time_line.h"

#include <como/render/gl/interface/utils.h>
#include <como_export.h>

#include <QEasingCurve>
#include <QSharedPointer>

namespace como
{

/**
 * Wraps effects->setActiveFullScreenEffect for the duration of it's lifespan
 */
class FullScreenEffectLock
{
public:
    FullScreenEffectLock(Effect* effect);
    ~FullScreenEffectLock();

private:
    Q_DISABLE_COPY(FullScreenEffectLock)
};

/**
 * References the previous window pixmap to prevent discarding.
 */
class PreviousWindowPixmapLock
{
public:
    PreviousWindowPixmapLock(EffectWindow* w);
    ~PreviousWindowPixmapLock();

private:
    EffectWindow* m_window;
    Q_DISABLE_COPY(PreviousWindowPixmapLock)
};
typedef QSharedPointer<PreviousWindowPixmapLock> PreviousWindowPixmapLockPtr;

class COMO_EXPORT AniData
{
public:
    AniData();
    AniData(AnimationEffect::Attribute a,
            int meta,
            const FPx2& to,
            int delay,
            const FPx2& from,
            bool waitAtSource,
            std::shared_ptr<FullScreenEffectLock> const& fullScreenEffectLock = nullptr,
            bool keepAlive = true,
            PreviousWindowPixmapLockPtr previousWindowPixmapLock = {},
            GLShader* shader = nullptr);

    bool isActive() const;

    inline bool isOneDimensional() const
    {
        return from[0] == from[1] && to[0] == to[1];
    }

    quint64 id{0};
    QString debugInfo() const;
    AnimationEffect::Attribute attribute;
    int customCurve;
    FPx2 from, to;
    TimeLine timeLine;
    uint meta;
    qint64 startTime;
    qint64 frozenTime;
    std::shared_ptr<FullScreenEffectLock> fullScreenEffectLock;
    bool waitAtSource;
    bool keepAlive;
    EffectWindowDeletedRef deletedRef;
    EffectWindowVisibleRef visibleRef;
    PreviousWindowPixmapLockPtr previousWindowPixmapLock;
    AnimationEffect::TerminationFlags terminationFlags;
    GLShader* shader{nullptr};
};

} // namespace

QDebug operator<<(QDebug dbg, const como::AniData& a);

#endif // ANIDATA_H
