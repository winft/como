/*
    SPDX-FileCopyrightText: 2019 Martin Flöser <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only
*/
#pragma once
#include <QObject>
#include <kwindowsystem_version.h>
#include <private/kwindoweffects_p.h>

namespace como
{

class WindowEffects : public QObject, public KWindowEffectsPrivate
{
public:
    WindowEffects();
    ~WindowEffects() override;

    bool isEffectAvailable(KWindowEffects::Effect effect) override;
    void
    slideWindow(QWindow* window, KWindowEffects::SlideFromLocation location, int offset) override;
    void enableBlurBehind(QWindow* window,
                          bool enable = true,
                          const QRegion& region = QRegion()) override;
    void enableBackgroundContrast(QWindow* window,
                                  bool enable = true,
                                  qreal contrast = 1,
                                  qreal intensity = 1,
                                  qreal saturation = 1,
                                  const QRegion& region = QRegion()) override;
};

}
