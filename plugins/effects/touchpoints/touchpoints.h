/*
SPDX-FileCopyrightText: 2012 Filip Wieladek <wattos@gmail.com>
SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>

SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef KWIN_TOUCHPOINTS_H
#define KWIN_TOUCHPOINTS_H

#include <render/effect/interface/effect.h>

namespace como
{

class TouchPointsEffect : public Effect
{
    Q_OBJECT
    Q_PROPERTY(qreal lineWidth READ lineWidth)
    Q_PROPERTY(int ringLife READ ringLife)
    Q_PROPERTY(int ringSize READ ringSize)
    Q_PROPERTY(int ringCount READ ringCount)
public:
    TouchPointsEffect();
    ~TouchPointsEffect() override;
    void prePaintScreen(effect::screen_prepaint_data& data) override;
    void paintScreen(effect::screen_paint_data& data) override;
    void postPaintScreen() override;
    bool isActive() const override;
    bool touchDown(qint32 id, const QPointF& pos, quint32 time) override;
    bool touchMotion(qint32 id, const QPointF& pos, quint32 time) override;
    bool touchUp(qint32 id, quint32 time) override;

    // for properties
    qreal lineWidth() const
    {
        return m_lineWidth;
    }
    int ringLife() const
    {
        return m_ringLife;
    }
    int ringSize() const
    {
        return m_ringMaxSize;
    }
    int ringCount() const
    {
        return m_ringCount;
    }

private:
    inline void drawCircle(const QColor& color, float cx, float cy, float r);
    inline void paintScreenSetup(effect::screen_paint_data const& data);
    inline void paintScreenFinish(effect::screen_paint_data const& data);

    void repaint();

    float computeAlpha(int time, int ring);
    float computeRadius(int time, bool press, int ring);
    void drawCircleGl(const QColor& color, float cx, float cy, float r);
    void drawCircleQPainter(const QColor& color, float cx, float cy, float r);
    void paintScreenSetupGl(effect::screen_paint_data const& data);
    void paintScreenFinishGl(effect::screen_paint_data const& data);

    Qt::GlobalColor colorForId(quint32 id);

    int m_ringCount = 2;
    float m_lineWidth = 1.0;
    int m_ringLife = 300;
    float m_ringMaxSize = 20.0;

    struct TouchPoint {
        QPointF pos;
        int time = 0;
        bool press;
        QColor color;
    };
    QVector<TouchPoint> m_points;
    QHash<quint32, QPointF> m_latestPositions;
    QHash<quint32, Qt::GlobalColor> m_colors;
    std::chrono::milliseconds m_lastPresentTime = std::chrono::milliseconds::zero();
};

} // namespace

#endif
