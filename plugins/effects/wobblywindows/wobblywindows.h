/*
    SPDX-FileCopyrightText: 2008 Cédric Borgese <cedric.borgese@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef KWIN_WOBBLYWINDOWS_H
#define KWIN_WOBBLYWINDOWS_H

#include <como/render/effect/interface/offscreen_effect.h>

namespace como
{

struct ParameterSet;

/**
 * Effect which wobble windows
 */
class WobblyWindowsEffect : public OffscreenEffect
{
    Q_OBJECT
    Q_PROPERTY(qreal stiffness READ stiffness)
    Q_PROPERTY(qreal drag READ drag)
    Q_PROPERTY(qreal moveFactor READ moveFactor)
    Q_PROPERTY(qreal xTesselation READ xTesselation)
    Q_PROPERTY(qreal yTesselation READ yTesselation)
    Q_PROPERTY(qreal minVelocity READ minVelocity)
    Q_PROPERTY(qreal maxVelocity READ maxVelocity)
    Q_PROPERTY(qreal stopVelocity READ stopVelocity)
    Q_PROPERTY(qreal minAcceleration READ minAcceleration)
    Q_PROPERTY(qreal maxAcceleration READ maxAcceleration)
    Q_PROPERTY(qreal stopAcceleration READ stopAcceleration)
    Q_PROPERTY(bool moveWobble READ isMoveWobble)
    Q_PROPERTY(bool resizeWobble READ isResizeWobble)
public:
    WobblyWindowsEffect();
    ~WobblyWindowsEffect() override;

    void reconfigure(ReconfigureFlags) override;
    void prePaintScreen(effect::screen_prepaint_data& data) override;
    void prePaintWindow(effect::window_prepaint_data& data) override;
    void postPaintScreen() override;
    bool isActive() const override;

    int requestedEffectChainPosition() const override
    {
        // Please notice that the Wobbly Windows effect has to be placed
        // after the Maximize effect in the effect chain, otherwise there
        // can be visual artifacts when dragging maximized windows.
        return 70;
    }

    // Wobbly model parameters
    void setStiffness(qreal stiffness);
    void setDrag(qreal drag);
    void setVelocityThreshold(qreal velocityThreshold);
    void setMoveFactor(qreal factor);

    struct Pair {
        qreal x;
        qreal y;
    };

    enum WindowStatus {
        Free,
        Moving,
    };

    static bool supported();

    // for properties
    qreal stiffness() const;
    qreal drag() const;
    qreal moveFactor() const;
    qreal xTesselation() const;
    qreal yTesselation() const;
    qreal minVelocity() const;
    qreal maxVelocity() const;
    qreal stopVelocity() const;
    qreal minAcceleration() const;
    qreal maxAcceleration() const;
    qreal stopAcceleration() const;
    bool isMoveWobble() const;
    bool isResizeWobble() const;

protected:
    void apply(effect::window_paint_data& data, WindowQuadList& quads) override;

public Q_SLOTS:
    void slotWindowAdded(como::EffectWindow* w);
    void slotWindowStartUserMovedResized(como::EffectWindow* w);
    void slotWindowStepUserMovedResized(como::EffectWindow* w, const QRect& geometry);
    void slotWindowFinishUserMovedResized(como::EffectWindow* w);
    void slotWindowMaximizeStateChanged(como::EffectWindow* w, bool horizontal, bool vertical);

private:
    void startMovedResized(EffectWindow* w);
    void stepMovedResized(EffectWindow* w);
    bool updateWindowWobblyDatas(EffectWindow* w, qreal time);

    struct WindowWobblyInfos {
        QVector<Pair> origin;
        QVector<Pair> position;
        QVector<Pair> velocity;
        QVector<Pair> acceleration;
        QVector<Pair> buffer;

        // if true, the physics system moves this point based only on it "normal" destination
        // given by the window position, ignoring neighbour points.
        QVector<bool> constraint;

        unsigned int width = 0;
        unsigned int height = 0;
        unsigned int count = 0;

        QVector<Pair> bezierSurface;
        unsigned int bezierWidth = 0;
        unsigned int bezierHeight = 0;
        unsigned int bezierCount = 0;

        WindowStatus status = Free;

        // for resizing. Only sides that have moved will wobble
        bool can_wobble_top = false;
        bool can_wobble_left = false;
        bool can_wobble_right = false;
        bool can_wobble_bottom = false;

        QRect resize_original_rect;

        std::chrono::milliseconds clock;
    };

    QHash<const EffectWindow*, WindowWobblyInfos> windows;

    QRegion m_updateRegion;

    qreal m_stiffness;
    qreal m_drag;
    qreal m_move_factor;

    // the default tesselation for windows
    // use qreal instead of int as I really often need
    // these values as real to do divisions.
    qreal m_xTesselation;
    qreal m_yTesselation;

    qreal m_minVelocity;
    qreal m_maxVelocity;
    qreal m_stopVelocity;
    qreal m_minAcceleration;
    qreal m_maxAcceleration;
    qreal m_stopAcceleration;

    bool m_moveWobble;
    bool m_resizeWobble;

    void initWobblyInfo(WindowWobblyInfos& wwi, QRect geometry) const;

    WobblyWindowsEffect::Pair computeBezierPoint(const WindowWobblyInfos& wwi, Pair point) const;

    static void heightRingLinearMean(QVector<Pair>& data, WindowWobblyInfos& wwi);

    void setParameterSet(const ParameterSet& pset);
};

}

#endif // WOBBLYWINDOWS_H
