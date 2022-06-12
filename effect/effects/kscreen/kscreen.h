/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

 Copyright (C) 2013 Martin Gräßlin <mgraesslin@kde.org>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/
#ifndef KWIN_KSCREEN_H
#define KWIN_KSCREEN_H

#include <kwineffects/effect.h>
#include <kwineffects/effect_window.h>
#include <kwineffects/paint_data.h>
#include <kwineffects/time_line.h>

namespace KWin
{

class KscreenEffect : public Effect
{
    Q_OBJECT

public:
    KscreenEffect();
    ~KscreenEffect() override;

    void prePaintScreen(ScreenPrePaintData& data, std::chrono::milliseconds presentTime) override;
    void postPaintScreen() override;
    void prePaintWindow(EffectWindow* w,
                        WindowPrePaintData& data,
                        std::chrono::milliseconds presentTime) override;
    void paintWindow(EffectWindow* w, int mask, QRegion region, WindowPaintData& data) override;

    void reconfigure(ReconfigureFlags flags) override;
    bool isActive() const override;

    int requestedEffectChainPosition() const override
    {
        return 99;
    }

    enum FadeOutState {
        StateNormal,
        StateFadingOut,
        StateFadedOut,
        StateFadingIn,
    };

    FadeOutState m_state{StateNormal};
    TimeLine m_timeLine;

private:
    void switchState();
};

} // namespace KWin
#endif // KWIN_KSCREEN_H
