/*
SPDX-FileCopyrightText: 2007 Lubos Lunak <l.lunak@kde.org>

SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef KWIN_SHOWPAINT_H
#define KWIN_SHOWPAINT_H

#include <como/render/effect/interface/effect.h>

namespace como
{

class ShowPaintEffect : public Effect
{
    Q_OBJECT

public:
    ShowPaintEffect();

    void paintScreen(effect::screen_paint_data& data) override;
    void paintWindow(effect::window_paint_data& data) override;

    bool isActive() const override;

private Q_SLOTS:
    void toggle();

private:
    void paintGL(const QMatrix4x4& projection);
    void paintQPainter();

    bool m_active = false;
    QRegion m_painted; // what's painted in one pass
    int m_colorIndex = 0;
};

}

#endif
