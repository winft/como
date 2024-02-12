/*
    SPDX-FileCopyrightText: 2023 Fushan Wen <qydwhotmail@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <como/render/effect/interface/effect_window.h>
#include <como/render/effect/interface/offscreen_effect.h>
#include <como/render/gl/interface/shader.h>

#include <unordered_set>

namespace como
{

/**
 * The color filter supports protanopia, deuteranopia and tritanopia.
 */
class ColorBlindnessCorrectionEffect : public OffscreenEffect
{
    Q_OBJECT

public:
    enum Mode {
        Protanopia = 0, //<Greatly reduced reds
        Deuteranopia,   //<Greatly reduced greens
        Tritanopia,     //<Greatly reduced blues
    };

    explicit ColorBlindnessCorrectionEffect();
    ~ColorBlindnessCorrectionEffect() override;

    bool isActive() const override;
    bool provides(Feature) override;
    void reconfigure(ReconfigureFlags flags) override;
    int requestedEffectChainPosition() const override;

    static bool supported();

public Q_SLOTS:
    void slotWindowDeleted(como::EffectWindow* w);

private Q_SLOTS:
    void correctColor(como::EffectWindow* w);

private:
    void loadData();

    Mode m_mode = Protanopia;

    std::unordered_set<como::EffectWindow*> m_windows;
    std::unique_ptr<GLShader> m_shader;
};

}
