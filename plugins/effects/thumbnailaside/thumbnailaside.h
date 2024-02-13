/*
SPDX-FileCopyrightText: 2007 Lubos Lunak <l.lunak@kde.org>
SPDX-FileCopyrightText: 2007 Christian Nitschkowski <christian.nitschkowski@kdemail.net>

SPDX-License-Identifier: GPL-2.0-or-later
*/

/*

 Testing of painting a window more than once.

*/

#ifndef KWIN_THUMBNAILASIDE_H
#define KWIN_THUMBNAILASIDE_H

#include <render/effect/interface/effect.h>

#include <QHash>

namespace como
{

class ThumbnailAsideEffect : public Effect
{
    Q_OBJECT
    Q_PROPERTY(int maxWidth READ configuredMaxWidth)
    Q_PROPERTY(int spacing READ configuredSpacing)
    Q_PROPERTY(qreal opacity READ configuredOpacity)
    Q_PROPERTY(int screen READ configuredScreen)
public:
    ThumbnailAsideEffect();
    void reconfigure(ReconfigureFlags) override;
    void paintScreen(effect::screen_paint_data& data) override;
    void paintWindow(effect::window_paint_data& data) override;

    // for properties
    int configuredMaxWidth() const
    {
        return maxwidth;
    }
    int configuredSpacing() const
    {
        return spacing;
    }
    qreal configuredOpacity() const
    {
        return opacity;
    }
    int configuredScreen() const
    {
        return screen;
    }
    bool isActive() const override;

private Q_SLOTS:
    void toggleCurrentThumbnail();
    void slotWindowAdded(como::EffectWindow* w);
    void slotWindowClosed(como::EffectWindow* w);
    void slotWindowFrameGeometryChanged(como::EffectWindow* w, const QRect& old);
    void slotWindowDamaged(como::EffectWindow* w, QRegion const& damage);
    void repaintAll();

private:
    void addThumbnail(EffectWindow* w);
    void removeThumbnail(EffectWindow* w);
    void arrange();
    struct Data {
        EffectWindow* window; // the same like the key in the hash (makes code simpler)
        int index;
        QRect rect;
    };
    QHash<EffectWindow*, Data> windows;
    int maxwidth;
    int spacing;
    double opacity;
    int screen;
    QRegion painted;
};

} // namespace

#endif
