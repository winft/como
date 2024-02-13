/*
SPDX-FileCopyrightText: 2007 Lubos Lunak <l.lunak@kde.org>

SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "fallapart.h"
// KConfigSkeleton
#include "fallapartconfig.h"

#include <como/render/effect/interface/effect_window.h>
#include <como/render/effect/interface/effects_handler.h>
#include <como/render/effect/interface/paint_data.h>
#include <como/render/effect/interface/types.h>

#include <QLoggingCategory>
#include <cmath>

Q_LOGGING_CATEGORY(KWIN_FALLAPART, "kwin_effect_fallapart", QtWarningMsg)

static const QSet<QString> s_blacklist{
    // Spectacle needs to be blacklisted in order to stay out of its own screenshots.
    QStringLiteral("spectacle spectacle"),         // x11
    QStringLiteral("spectacle org.kde.spectacle"), // wayland
};

namespace como
{

bool FallApartEffect::supported()
{
    return OffscreenEffect::supported() && effects->animationsSupported();
}

FallApartEffect::FallApartEffect()
{
    FallApartConfig::instance(effects->config());
    reconfigure(ReconfigureAll);
    connect(effects, &EffectsHandler::windowClosed, this, &FallApartEffect::slotWindowClosed);
    connect(effects, &EffectsHandler::windowDeleted, this, &FallApartEffect::slotWindowDeleted);
    connect(
        effects, &EffectsHandler::windowDataChanged, this, &FallApartEffect::slotWindowDataChanged);
}

void FallApartEffect::reconfigure(ReconfigureFlags)
{
    FallApartConfig::self()->read();
    blockSize = FallApartConfig::blockSize();
}

void FallApartEffect::prePaintScreen(effect::screen_prepaint_data& data)
{
    if (!windows.isEmpty()) {
        data.paint.mask |= PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS;
    }
    effects->prePaintScreen(data);
}

void FallApartEffect::prePaintWindow(effect::window_prepaint_data& data)
{
    auto animationIt = windows.find(&data.window);
    if (animationIt != windows.end() && isRealWindow(&data.window)) {
        int time = 0;
        if (animationIt->lastPresentTime.count()) {
            time = (data.present_time - animationIt->lastPresentTime).count();
        }
        animationIt->lastPresentTime = data.present_time;

        animationIt->progress += time / animationTime(1000.);
        data.paint.mask |= Effect::PAINT_WINDOW_TRANSFORMED;
    }
    effects->prePaintWindow(data);
}

void FallApartEffect::apply(effect::window_paint_data& data, WindowQuadList& quads)
{
    auto animationIt = windows.constFind(&data.window);
    if (animationIt != windows.constEnd() && isRealWindow(&data.window)) {
        const qreal t = animationIt->progress;

        // Request the window to be divided into cells
        quads = quads.makeGrid(blockSize);
        int cnt = 0;

        for (WindowQuad& quad : quads) {
            // make fragments move in various directions, based on where
            // they are (left pieces generally move to the left, etc.)
            QPointF p1(quad[0].x(), quad[0].y());
            double xdiff = 0;

            if (p1.x() < data.window.width() / 2) {
                xdiff = -(data.window.width() / 2 - p1.x()) / data.window.width() * 100;
            }
            if (p1.x() > data.window.width() / 2) {
                xdiff = (p1.x() - data.window.width() / 2) / data.window.width() * 100;
            }
            double ydiff = 0;
            if (p1.y() < data.window.height() / 2) {
                ydiff = -(data.window.height() / 2 - p1.y()) / data.window.height() * 100;
            }
            if (p1.y() > data.window.height() / 2) {
                ydiff = (p1.y() - data.window.height() / 2) / data.window.height() * 100;
            }

            double modif = t * t * 64;
            srandom(cnt); // change direction randomly but consistently
            xdiff += (rand() % 21 - 10);
            ydiff += (rand() % 21 - 10);

            for (int j = 0; j < 4; ++j) {
                quad[j].move(quad[j].x() + xdiff * modif, quad[j].y() + ydiff * modif);
            }

            // also make the fragments rotate around their center
            QPointF center((quad[0].x() + quad[1].x() + quad[2].x() + quad[3].x()) / 4,
                           (quad[0].y() + quad[1].y() + quad[2].y() + quad[3].y()) / 4);

            // spin randomly
            double adiff = (rand() % 720 - 360) / 360. * 2 * M_PI;

            for (int j = 0; j < 4; ++j) {
                double x = quad[j].x() - center.x();
                double y = quad[j].y() - center.y();
                double angle = atan2(y, x);
                angle += animationIt->progress * adiff;
                double dist = sqrt(x * x + y * y);
                x = dist * cos(angle);
                y = dist * sin(angle);
                quad[j].move(center.x() + x, center.y() + y);
            }
            ++cnt;
        }

        data.paint.opacity *= interpolate(1.0, 0.0, t);
    }
}

void FallApartEffect::postPaintScreen()
{
    for (auto it = windows.begin(); it != windows.end();) {
        if (it->progress < 1) {
            ++it;
        } else {
            unredirect(it.key());
            it = windows.erase(it);
        }
    }

    effects->addRepaintFull();
    effects->postPaintScreen();
}

bool FallApartEffect::isRealWindow(EffectWindow* w)
{
    // TODO: isSpecialWindow is rather generic, maybe tell windowtypes separately?
    /*
    qCDebug(KWIN_FALLAPART) << "--" << w->caption() << "--------------------------------";
    qCDebug(KWIN_FALLAPART) << "Tooltip:" << w->isTooltip();
    qCDebug(KWIN_FALLAPART) << "Toolbar:" << w->isToolbar();
    qCDebug(KWIN_FALLAPART) << "Desktop:" << w->isDesktop();
    qCDebug(KWIN_FALLAPART) << "Special:" << w->isSpecialWindow();
    qCDebug(KWIN_FALLAPART) << "TopMenu:" << w->isTopMenu();
    qCDebug(KWIN_FALLAPART) << "Notific:" << w->isNotification();
    qCDebug(KWIN_FALLAPART) << "Splash:" << w->isSplash();
    qCDebug(KWIN_FALLAPART) << "Normal:" << w->isNormalWindow();
    */
    if (w->isPopupWindow()) {
        return false;
    }
    if (w->isX11Client() && !w->isManaged()) {
        return false;
    }
    if (!w->isNormalWindow())
        return false;
    return true;
}

void FallApartEffect::slotWindowClosed(EffectWindow* c)
{
    if (effects->activeFullScreenEffect())
        return;
    if (!isRealWindow(c))
        return;
    if (!c->isVisible())
        return;
    if (s_blacklist.contains(c->windowClass())) {
        return;
    }
    const void* e = c->data(WindowClosedGrabRole).value<void*>();
    if (e && e != this)
        return;
    c->setData(WindowClosedGrabRole, QVariant::fromValue(static_cast<void*>(this)));

    auto& animation = windows[c];
    animation.progress = 0;
    animation.deletedRef = EffectWindowDeletedRef(c);
    animation.visibleRef = EffectWindowVisibleRef(c, EffectWindow::PAINT_DISABLED_BY_DELETE);

    redirect(c);
}

void FallApartEffect::slotWindowDeleted(EffectWindow* c)
{
    windows.remove(c);
}

void FallApartEffect::slotWindowDataChanged(EffectWindow* w, int role)
{
    if (role != WindowClosedGrabRole) {
        return;
    }

    if (w->data(role).value<void*>() == this) {
        return;
    }

    auto it = windows.find(w);
    if (it == windows.end()) {
        return;
    }

    unredirect(it.key());
    windows.erase(it);
}

bool FallApartEffect::isActive() const
{
    return !windows.isEmpty();
}

} // namespace
