/*
    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>

SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef KWIN_COLORPICKER_H
#define KWIN_COLORPICKER_H

#include <render/effect/interface/effect.h>
#include <render/effect/interface/effect_screen.h>

#include <QColor>
#include <QDBusContext>
#include <QDBusMessage>
#include <QDBusUnixFileDescriptor>
#include <QObject>

namespace como
{

class ColorPickerEffect : public Effect, protected QDBusContext
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.kwin.ColorPicker")
public:
    ColorPickerEffect();
    ~ColorPickerEffect() override;
    void paintScreen(effect::screen_paint_data& data) override;
    bool isActive() const override;

    int requestedEffectChainPosition() const override
    {
        return 0;
    }

    static bool supported();

public Q_SLOTS:
    Q_SCRIPTABLE QColor pick();

private:
    void showInfoMessage();
    void hideInfoMessage();

    QDBusMessage m_replyMessage;
    QPoint m_scheduledPosition;
    bool m_picking = false;
};

} // namespace

#endif
