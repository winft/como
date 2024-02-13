/*
SPDX-FileCopyrightText: 2009 Martin Gräßlin <mgraesslin@kde.org>

SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef KWIN_RESIZE_H
#define KWIN_RESIZE_H

#include <render/effect/interface/animation_effect.h>

namespace como
{

class ResizeEffect : public AnimationEffect
{
    Q_OBJECT
    Q_PROPERTY(bool textureScale READ isTextureScale)
    Q_PROPERTY(bool outline READ isOutline)
public:
    ResizeEffect();
    ~ResizeEffect() override;
    inline bool provides(Effect::Feature ef) override
    {
        return ef == Effect::Resize;
    }
    inline bool isActive() const override
    {
        return m_active || AnimationEffect::isActive();
    }
    void prePaintScreen(effect::screen_prepaint_data& data) override;
    void prePaintWindow(effect::window_prepaint_data& data) override;
    void paintWindow(effect::window_paint_data& data) override;
    void reconfigure(ReconfigureFlags) override;

    int requestedEffectChainPosition() const override
    {
        return 60;
    }

    bool isTextureScale() const
    {
        return m_features & TextureScale;
    }
    bool isOutline() const
    {
        return m_features & Outline;
    }

public Q_SLOTS:
    void slotWindowAdded(como::EffectWindow* w);
    void slotWindowStartUserMovedResized(como::EffectWindow* w);
    void slotWindowStepUserMovedResized(como::EffectWindow* w, const QRect& geometry);
    void slotWindowFinishUserMovedResized(como::EffectWindow* w);

private:
    enum Feature { TextureScale = 1 << 0, Outline = 1 << 1 };
    bool m_active;
    int m_features;
    EffectWindow* m_resizeWindow;
    QRect m_currentGeometry, m_originalGeometry;
};

}

#endif
