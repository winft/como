/*
SPDX-FileCopyrightText: 2009 Lucas Murray <lmurray@undefinedfire.com>

SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef KWIN_HIGHLIGHTWINDOW_H
#define KWIN_HIGHLIGHTWINDOW_H

#include <como/render/effect/interface/animation_effect.h>

namespace como
{

class HighlightWindowEffect : public AnimationEffect
{
    Q_OBJECT

public:
    HighlightWindowEffect();
    ~HighlightWindowEffect() override;

    int requestedEffectChainPosition() const override
    {
        return 70;
    }

    bool provides(Feature feature) override;
    bool perform(Feature feature, const QVariantList& arguments) override;
    Q_SCRIPTABLE void highlightWindows(const QStringList& windows);

public Q_SLOTS:
    void slotWindowAdded(como::EffectWindow* w);
    void slotWindowClosed(como::EffectWindow* w);
    void slotWindowDeleted(como::EffectWindow* w);

private:
    quint64 startGhostAnimation(EffectWindow* window);
    quint64 startHighlightAnimation(EffectWindow* window);
    void startRevertAnimation(EffectWindow* window);

    bool isHighlighted(EffectWindow* window) const;

    void prepareHighlighting();
    void finishHighlighting();
    void highlightWindows(const QVector<como::EffectWindow*>& windows);

    QList<EffectWindow*> m_highlightedWindows;
    QHash<EffectWindow*, quint64> m_animations;
    QEasingCurve m_easingCurve;
    int m_fadeDuration;
    EffectWindow* m_monitorWindow;
    QList<WId> m_highlightedIds;
    float m_ghostOpacity = 0;
};

} // namespace

#endif
