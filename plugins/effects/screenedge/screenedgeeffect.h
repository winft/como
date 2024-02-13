/*
SPDX-FileCopyrightText: 2013 Martin Gräßlin <mgraesslin@kde.org>

SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef KWIN_SCREEN_EDGE_EFFECT_H
#define KWIN_SCREEN_EDGE_EFFECT_H

#include <render/effect/interface/effect.h>

#include <KConfigWatcher>
#include <memory>

class QTimer;
namespace KSvg
{
class Svg;
}

namespace como
{
class Glow;
class GLTexture;

class ScreenEdgeEffect : public Effect
{
    Q_OBJECT
public:
    ScreenEdgeEffect();
    ~ScreenEdgeEffect() override;
    void prePaintScreen(effect::screen_prepaint_data& data) override;
    void paintScreen(effect::screen_paint_data& data) override;
    bool isActive() const override;

    int requestedEffectChainPosition() const override
    {
        return 10;
    }

private Q_SLOTS:
    void edgeApproaching(ElectricBorder border, qreal factor, const QRect& geometry);
    void cleanup();

private:
    void ensureGlowSvg();
    std::unique_ptr<Glow> createGlow(ElectricBorder border, qreal factor, const QRect& geometry);
    template<typename T>
    T* createCornerGlow(ElectricBorder border);
    template<typename T>
    T* createEdgeGlow(ElectricBorder border, const QSize& size);
    QSize cornerGlowSize(ElectricBorder border);
    KConfigWatcher::Ptr m_configWatcher;
    KSvg::Svg* m_glow = nullptr;
    std::map<ElectricBorder, std::unique_ptr<Glow>> m_borders;
    QTimer* m_cleanupTimer;
};

class Glow
{
public:
    QScopedPointer<GLTexture> texture;
    QScopedPointer<QImage> image;
    QSize pictureSize;
    qreal strength;
    QRect geometry;
    ElectricBorder border;
};

}

#endif
