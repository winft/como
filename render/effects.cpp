/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2006 Lubos Lunak <l.lunak@kde.org>
Copyright (C) 2010, 2011 Martin Gräßlin <mgraesslin@kde.org>

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
#include "effects.h"

#include "compositor.h"
#include "effect_loader.h"
#include "effectsadaptor.h"
#include "gl/backend.h"
#include "gl/scene.h"
#include "platform.h"
#include "thumbnail_item.h"
#include "x11/effect.h"
#include "x11/property_notify_filter.h"

#include "base/logging.h"
#include "base/output.h"
#include "base/platform.h"
#include "desktop/screen_locker_watcher.h"
#include "input/cursor.h"
#include "input/pointer_redirect.h"
#include "scripting/effect.h"
#include "win/activation.h"
#include "win/control.h"
#include "win/deco/bridge.h"
#include "win/desktop_get.h"
#include "win/internal_window.h"
#include "win/meta.h"
#include "win/osd.h"
#include "win/remnant.h"
#include "win/screen.h"
#include "win/screen_edges.h"
#include "win/space.h"
#include "win/stacking_order.h"
#include "win/transient.h"
#include "win/virtual_desktops.h"
#include "win/window_area.h"
#include "win/x11/group.h"
#include "win/x11/stacking.h"
#include "win/x11/window.h"
#include "win/x11/window_find.h"

#if KWIN_BUILD_TABBOX
#include "win/tabbox/tabbox.h"
#endif

#include <kwingl/platform.h>
#include <kwingl/utils.h>

#include <KDecoration2/DecorationSettings>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QStandardPaths>

namespace KWin::render
{
//---------------------
// Static

static QByteArray readWindowProperty(xcb_window_t win, xcb_atom_t atom, xcb_atom_t type, int format)
{
    if (win == XCB_WINDOW_NONE) {
        return QByteArray();
    }
    uint32_t len = 32768;
    for (;;) {
        base::x11::xcb::property prop(false, win, atom, XCB_ATOM_ANY, 0, len);
        if (prop.is_null()) {
            // get property failed
            return QByteArray();
        }
        if (prop->bytes_after > 0) {
            len *= 2;
            continue;
        }
        return prop.to_byte_array(format, type);
    }
}

static void deleteWindowProperty(xcb_window_t win, long int atom)
{
    if (win == XCB_WINDOW_NONE) {
        return;
    }
    xcb_delete_property(kwinApp()->x11Connection(), win, atom);
}

//---------------------

effects_handler_impl::effects_handler_impl(render::compositor* compositor, render::scene* scene)
    : EffectsHandler(scene->compositingType())
    , m_compositor(compositor)
    , m_scene(scene)
    , m_desktopRendering(false)
    , m_currentRenderedDesktop(0)
    , m_effectLoader(new effect_loader(*compositor->space, this))
    , m_trackingCursorChanges(0)
{
    qRegisterMetaType<QVector<KWin::EffectWindow*>>();
    connect(m_effectLoader,
            &basic_effect_loader::effectLoaded,
            this,
            [this](Effect* effect, const QString& name) {
                effect_order.insert(effect->requestedEffectChainPosition(),
                                    EffectPair(name, effect));
                loaded_effects << EffectPair(name, effect);
                effectsChanged();
            });
    m_effectLoader->setConfig(kwinApp()->config());
    new EffectsAdaptor(this);
    QDBusConnection dbus = QDBusConnection::sessionBus();
    dbus.registerObject(QStringLiteral("/Effects"), this);
    // init is important, otherwise causes crashes when quads are build before the first painting
    // pass start
    m_currentBuildQuadsIterator = m_activeEffects.constEnd();

    auto ws = compositor->space;
    auto& vds = ws->virtual_desktop_manager;
    connect(ws->qobject.get(),
            &win::space::qobject_t::showingDesktopChanged,
            this,
            &effects_handler_impl::showingDesktopChanged);
    connect(ws->qobject.get(),
            &win::space::qobject_t::currentDesktopChanged,
            this,
            [this](int old, Toplevel* c) {
                int const newDesktop = m_compositor->space->virtual_desktop_manager->current();
                if (old != 0 && newDesktop != old) {
                    assert(!c || c->render);
                    assert(!c || c->render->effect);
                    auto eff_win = c ? c->render->effect.get() : nullptr;
                    Q_EMIT desktopChanged(old, newDesktop, eff_win);
                    // TODO: remove in 4.10
                    Q_EMIT desktopChanged(old, newDesktop);
                }
            });
    connect(ws->qobject.get(),
            &win::space::qobject_t::desktopPresenceChanged,
            this,
            [this](Toplevel* c, int old) {
                assert(c);
                assert(c->render);
                assert(c->render->effect);
                Q_EMIT desktopPresenceChanged(c->render->effect.get(), old, c->desktop());
            });
    connect(ws->qobject.get(), &win::space::qobject_t::clientAdded, this, [this](auto c) {
        if (c->ready_for_painting)
            slotClientShown(c);
        else
            connect(c, &Toplevel::windowShown, this, &effects_handler_impl::slotClientShown);
    });
    connect(ws->qobject.get(), &win::space::qobject_t::unmanagedAdded, this, [this](Toplevel* u) {
        // it's never initially ready but has synthetic 50ms delay
        connect(u, &Toplevel::windowShown, this, &effects_handler_impl::slotUnmanagedShown);
    });
    connect(
        ws->qobject.get(), &win::space::qobject_t::internalClientAdded, this, [this](auto client) {
            assert(client->render);
            assert(client->render->effect);
            setupAbstractClientConnections(client);
            Q_EMIT windowAdded(client->render->effect.get());
        });
    connect(ws->qobject.get(),
            &win::space::qobject_t::clientActivated,
            this,
            [this](KWin::Toplevel* window) {
                assert(!window || window->render);
                assert(!window || window->render->effect);
                auto eff_win = window ? window->render->effect.get() : nullptr;
                Q_EMIT windowActivated(eff_win);
            });
    connect(
        ws->qobject.get(), &win::space::qobject_t::window_deleted, this, [this](KWin::Toplevel* d) {
            assert(d->render);
            assert(d->render->effect);
            Q_EMIT windowDeleted(d->render->effect.get());
            elevated_windows.removeAll(d->render->effect.get());
        });
    connect(ws->session_manager.get(),
            &win::session_manager::stateChanged,
            this,
            &KWin::EffectsHandler::sessionStateChanged);
    connect(vds.get(),
            &win::virtual_desktop_manager::countChanged,
            this,
            &EffectsHandler::numberDesktopsChanged);
    QObject::connect(ws->input->platform.cursor.get(),
                     &input::cursor::mouse_changed,
                     this,
                     &EffectsHandler::mouseChanged);

    auto& base = kwinApp()->get_base();
    connect(&base, &base::platform::output_added, this, &EffectsHandler::numberScreensChanged);
    connect(&base, &base::platform::output_removed, this, &EffectsHandler::numberScreensChanged);

    QObject::connect(
        &base, &base::platform::topology_changed, this, [this](auto old_topo, auto new_topo) {
            if (old_topo.size != new_topo.size) {
                Q_EMIT virtualScreenSizeChanged();
                Q_EMIT virtualScreenGeometryChanged();
            }
        });

    connect(ws->stacking_order.get(),
            &win::stacking_order::changed,
            this,
            &EffectsHandler::stackingOrderChanged);
#if KWIN_BUILD_TABBOX
    auto tabBox = ws->tabbox.get();
    connect(tabBox, &win::tabbox::tabbox_added, this, &EffectsHandler::tabBoxAdded);
    connect(tabBox, &win::tabbox::tabbox_updated, this, &EffectsHandler::tabBoxUpdated);
    connect(tabBox, &win::tabbox::tabbox_closed, this, &EffectsHandler::tabBoxClosed);
    connect(tabBox, &win::tabbox::tabbox_key_event, this, &EffectsHandler::tabBoxKeyEvent);
#endif
    connect(ws->edges.get(),
            &win::screen_edger::approaching,
            this,
            &EffectsHandler::screenEdgeApproaching);
    connect(kwinApp()->screen_locker_watcher.get(),
            &desktop::screen_locker_watcher::locked,
            this,
            &EffectsHandler::screenLockingChanged);
    connect(kwinApp()->screen_locker_watcher.get(),
            &desktop::screen_locker_watcher::about_to_lock,
            this,
            &EffectsHandler::screenAboutToLock);

    auto make_property_filter = [this] {
        using filter = x11::property_notify_filter<effects_handler_impl, win::space>;
        x11_property_notify
            = std::make_unique<filter>(*this, *m_compositor->space, kwinApp()->x11RootWindow());
    };

    connect(kwinApp(), &Application::x11ConnectionChanged, this, [this, make_property_filter] {
        registered_atoms.clear();
        for (auto it = m_propertiesForEffects.keyBegin(); it != m_propertiesForEffects.keyEnd();
             it++) {
            x11::add_support_property(*this, *it);
        }
        if (kwinApp()->x11Connection()) {
            make_property_filter();
        } else {
            x11_property_notify.reset();
        }
        Q_EMIT xcbConnectionChanged();
    });

    if (kwinApp()->x11Connection()) {
        make_property_filter();
    }

    // connect all clients
    for (auto& window : ws->windows) {
        // TODO: Can we merge this with the one for Wayland XdgShellClients below?
        if (!window->control) {
            continue;
        }
        auto x11_client = qobject_cast<win::x11::window*>(window);
        if (!x11_client) {
            continue;
        }
        setupClientConnections(x11_client);
    }
    for (auto unmanaged : win::x11::get_unmanageds<Toplevel>(*ws)) {
        setupUnmanagedConnections(unmanaged);
    }
    for (auto window : ws->windows) {
        if (auto internal = qobject_cast<win::internal_window*>(window)) {
            setupAbstractClientConnections(internal);
        }
    }

    connect(&kwinApp()->get_base(),
            &base::platform::output_added,
            this,
            &effects_handler_impl::slotOutputEnabled);
    connect(&kwinApp()->get_base(),
            &base::platform::output_removed,
            this,
            &effects_handler_impl::slotOutputDisabled);

    auto const outputs = kwinApp()->get_base().get_outputs();
    for (base::output* output : outputs) {
        slotOutputEnabled(output);
    }
}

effects_handler_impl::~effects_handler_impl()
{
    unloadAllEffects();
}

void effects_handler_impl::unloadAllEffects()
{
    for (const EffectPair& pair : qAsConst(loaded_effects)) {
        destroyEffect(pair.second);
    }

    effect_order.clear();
    m_effectLoader->clear();

    effectsChanged();
}

void effects_handler_impl::setupAbstractClientConnections(Toplevel* window)
{
    connect(window, &Toplevel::remnant_created, this, &effects_handler_impl::add_remnant);
    connect(window,
            static_cast<void (Toplevel::*)(KWin::Toplevel*, win::maximize_mode)>(
                &Toplevel::clientMaximizedStateChanged),
            this,
            &effects_handler_impl::slotClientMaximized);
    connect(window, &Toplevel::clientStartUserMovedResized, this, [this](Toplevel* c) {
        Q_EMIT windowStartUserMovedResized(c->render->effect.get());
    });
    connect(window,
            &Toplevel::clientStepUserMovedResized,
            this,
            [this](Toplevel* c, const QRect& geometry) {
                Q_EMIT windowStepUserMovedResized(c->render->effect.get(), geometry);
            });
    connect(window, &Toplevel::clientFinishUserMovedResized, this, [this](Toplevel* c) {
        Q_EMIT windowFinishUserMovedResized(c->render->effect.get());
    });
    connect(window, &Toplevel::opacityChanged, this, &effects_handler_impl::slotOpacityChanged);
    connect(window, &Toplevel::clientMinimized, this, [this](Toplevel* c, bool animate) {
        // TODO: notify effects even if it should not animate?
        if (animate) {
            Q_EMIT windowMinimized(c->render->effect.get());
        }
    });
    connect(window, &Toplevel::clientUnminimized, this, [this](Toplevel* c, bool animate) {
        // TODO: notify effects even if it should not animate?
        if (animate) {
            Q_EMIT windowUnminimized(c->render->effect.get());
        }
    });
    connect(
        window, &Toplevel::modalChanged, this, &effects_handler_impl::slotClientModalityChanged);
    connect(window,
            &Toplevel::frame_geometry_changed,
            this,
            &effects_handler_impl::slotGeometryShapeChanged);
    connect(window,
            &Toplevel::frame_geometry_changed,
            this,
            &effects_handler_impl::slotFrameGeometryChanged);
    connect(window, &Toplevel::damaged, this, &effects_handler_impl::slotWindowDamaged);
    connect(window, &Toplevel::unresponsiveChanged, this, [this, window](bool unresponsive) {
        Q_EMIT windowUnresponsiveChanged(window->render->effect.get(), unresponsive);
    });
    connect(window, &Toplevel::windowShown, this, [this](Toplevel* c) {
        Q_EMIT windowShown(c->render->effect.get());
    });
    connect(window, &Toplevel::windowHidden, this, [this](Toplevel* c) {
        Q_EMIT windowHidden(c->render->effect.get());
    });
    connect(window, &Toplevel::keepAboveChanged, this, [this, window](bool above) {
        Q_UNUSED(above)
        Q_EMIT windowKeepAboveChanged(window->render->effect.get());
    });
    connect(window, &Toplevel::keepBelowChanged, this, [this, window](bool below) {
        Q_UNUSED(below)
        Q_EMIT windowKeepBelowChanged(window->render->effect.get());
    });
    connect(window, &Toplevel::fullScreenChanged, this, [this, window]() {
        Q_EMIT windowFullScreenChanged(window->render->effect.get());
    });
    connect(window, &Toplevel::visible_geometry_changed, this, [this, window]() {
        Q_EMIT windowExpandedGeometryChanged(window->render->effect.get());
    });
}

void effects_handler_impl::setupClientConnections(win::x11::window* c)
{
    setupAbstractClientConnections(c);
    connect(c, &win::x11::window::paddingChanged, this, &effects_handler_impl::slotPaddingChanged);
}

void effects_handler_impl::setupUnmanagedConnections(Toplevel* u)
{
    connect(u, &Toplevel::remnant_created, this, &effects_handler_impl::add_remnant);
    connect(u, &Toplevel::opacityChanged, this, &effects_handler_impl::slotOpacityChanged);
    connect(u,
            &Toplevel::frame_geometry_changed,
            this,
            &effects_handler_impl::slotGeometryShapeChanged);
    connect(u,
            &Toplevel::frame_geometry_changed,
            this,
            &effects_handler_impl::slotFrameGeometryChanged);
    connect(u, &Toplevel::paddingChanged, this, &effects_handler_impl::slotPaddingChanged);
    connect(u, &Toplevel::damaged, this, &effects_handler_impl::slotWindowDamaged);
    connect(u, &Toplevel::visible_geometry_changed, this, [this, u]() {
        Q_EMIT windowExpandedGeometryChanged(u->render->effect.get());
    });
}

void effects_handler_impl::reconfigure()
{
    m_effectLoader->queryAndLoadAll();
}

// the idea is that effects call this function again which calls the next one
void effects_handler_impl::prePaintScreen(ScreenPrePaintData& data,
                                          std::chrono::milliseconds presentTime)
{
    if (m_currentPaintScreenIterator != m_activeEffects.constEnd()) {
        (*m_currentPaintScreenIterator++)->prePaintScreen(data, presentTime);
        --m_currentPaintScreenIterator;
    }
    // no special final code
}

void effects_handler_impl::paintScreen(int mask, const QRegion& region, ScreenPaintData& data)
{
    if (m_currentPaintScreenIterator != m_activeEffects.constEnd()) {
        (*m_currentPaintScreenIterator++)->paintScreen(mask, region, data);
        --m_currentPaintScreenIterator;
    } else
        m_scene->finalPaintScreen(static_cast<render::paint_type>(mask), region, data);
}

void effects_handler_impl::paintDesktop(int desktop,
                                        int mask,
                                        QRegion region,
                                        ScreenPaintData& data)
{
    if (desktop < 1 || desktop > numberOfDesktops()) {
        return;
    }
    m_currentRenderedDesktop = desktop;
    m_desktopRendering = true;
    // save the paint screen iterator
    EffectsIterator savedIterator = m_currentPaintScreenIterator;
    m_currentPaintScreenIterator = m_activeEffects.constBegin();
    paintScreen(mask, region, data);
    // restore the saved iterator
    m_currentPaintScreenIterator = savedIterator;
    m_desktopRendering = false;
}

void effects_handler_impl::postPaintScreen()
{
    if (m_currentPaintScreenIterator != m_activeEffects.constEnd()) {
        (*m_currentPaintScreenIterator++)->postPaintScreen();
        --m_currentPaintScreenIterator;
    }
    // no special final code
}

void effects_handler_impl::prePaintWindow(EffectWindow* w,
                                          WindowPrePaintData& data,
                                          std::chrono::milliseconds presentTime)
{
    if (m_currentPaintWindowIterator != m_activeEffects.constEnd()) {
        (*m_currentPaintWindowIterator++)->prePaintWindow(w, data, presentTime);
        --m_currentPaintWindowIterator;
    }
    // no special final code
}

void effects_handler_impl::paintWindow(EffectWindow* w,
                                       int mask,
                                       const QRegion& region,
                                       WindowPaintData& data)
{
    if (m_currentPaintWindowIterator != m_activeEffects.constEnd()) {
        (*m_currentPaintWindowIterator++)->paintWindow(w, mask, region, data);
        --m_currentPaintWindowIterator;
    } else
        m_scene->finalPaintWindow(static_cast<effects_window_impl*>(w),
                                  static_cast<render::paint_type>(mask),
                                  region,
                                  data);
}

void effects_handler_impl::postPaintWindow(EffectWindow* w)
{
    if (m_currentPaintWindowIterator != m_activeEffects.constEnd()) {
        (*m_currentPaintWindowIterator++)->postPaintWindow(w);
        --m_currentPaintWindowIterator;
    }
    // no special final code
}

Effect* effects_handler_impl::provides(Effect::Feature ef)
{
    for (int i = 0; i < loaded_effects.size(); ++i)
        if (loaded_effects.at(i).second->provides(ef))
            return loaded_effects.at(i).second;
    return nullptr;
}

void effects_handler_impl::drawWindow(EffectWindow* w,
                                      int mask,
                                      const QRegion& region,
                                      WindowPaintData& data)
{
    if (m_currentDrawWindowIterator != m_activeEffects.constEnd()) {
        (*m_currentDrawWindowIterator++)->drawWindow(w, mask, region, data);
        --m_currentDrawWindowIterator;
    } else
        m_scene->finalDrawWindow(static_cast<effects_window_impl*>(w),
                                 static_cast<render::paint_type>(mask),
                                 region,
                                 data);
}

void effects_handler_impl::buildQuads(EffectWindow* w, WindowQuadList& quadList)
{
    static bool initIterator = true;
    if (initIterator) {
        m_currentBuildQuadsIterator = m_activeEffects.constBegin();
        initIterator = false;
    }
    if (m_currentBuildQuadsIterator != m_activeEffects.constEnd()) {
        (*m_currentBuildQuadsIterator++)->buildQuads(w, quadList);
        --m_currentBuildQuadsIterator;
    }
    if (m_currentBuildQuadsIterator == m_activeEffects.constBegin())
        initIterator = true;
}

bool effects_handler_impl::hasDecorationShadows() const
{
    return false;
}

bool effects_handler_impl::decorationsHaveAlpha() const
{
    return true;
}

// start another painting pass
void effects_handler_impl::startPaint()
{
    m_activeEffects.clear();
    m_activeEffects.reserve(loaded_effects.count());
    for (QVector<KWin::EffectPair>::const_iterator it = loaded_effects.constBegin();
         it != loaded_effects.constEnd();
         ++it) {
        if (it->second->isActive()) {
            m_activeEffects << it->second;
        }
    }
    m_currentDrawWindowIterator = m_activeEffects.constBegin();
    m_currentPaintWindowIterator = m_activeEffects.constBegin();
    m_currentPaintScreenIterator = m_activeEffects.constBegin();
}

void effects_handler_impl::slotClientMaximized(Toplevel* window, win::maximize_mode maxMode)
{
    bool horizontal = false;
    bool vertical = false;
    switch (maxMode) {
    case win::maximize_mode::horizontal:
        horizontal = true;
        break;
    case win::maximize_mode::vertical:
        vertical = true;
        break;
    case win::maximize_mode::full:
        horizontal = true;
        vertical = true;
        break;
    case win::maximize_mode::restore: // fall through
    default:
        // default - nothing to do
        break;
    }

    auto ew = window->render->effect.get();
    assert(ew);
    Q_EMIT windowMaximizedStateChanged(ew, horizontal, vertical);
}

void effects_handler_impl::slotOpacityChanged(Toplevel* t, qreal oldOpacity)
{
    assert(t->render->effect);

    if (t->opacity() == oldOpacity) {
        return;
    }

    Q_EMIT windowOpacityChanged(
        t->render->effect.get(), oldOpacity, static_cast<qreal>(t->opacity()));
}

void effects_handler_impl::slotClientShown(KWin::Toplevel* t)
{
    assert(qobject_cast<win::x11::window*>(t));
    auto c = static_cast<win::x11::window*>(t);
    disconnect(c, &Toplevel::windowShown, this, &effects_handler_impl::slotClientShown);
    setupClientConnections(c);
    Q_EMIT windowAdded(c->render->effect.get());
}

void effects_handler_impl::slotXdgShellClientShown(Toplevel* t)
{
    setupAbstractClientConnections(t);
    Q_EMIT windowAdded(t->render->effect.get());
}

void effects_handler_impl::slotUnmanagedShown(KWin::Toplevel* t)
{ // regardless, unmanaged windows are -yet?- not synced anyway
    assert(!t->control);
    setupUnmanagedConnections(t);
    Q_EMIT windowAdded(t->render->effect.get());
}

void effects_handler_impl::add_remnant(Toplevel* remnant)
{
    assert(remnant);
    assert(remnant->render);
    Q_EMIT windowClosed(remnant->render->effect.get());
}

void effects_handler_impl::slotClientModalityChanged()
{
    Q_EMIT windowModalityChanged(static_cast<win::x11::window*>(sender())->render->effect.get());
}

void effects_handler_impl::slotCurrentTabAboutToChange(EffectWindow* from, EffectWindow* to)
{
    Q_EMIT currentTabAboutToChange(from, to);
}

void effects_handler_impl::slotTabAdded(EffectWindow* w, EffectWindow* to)
{
    Q_EMIT tabAdded(w, to);
}

void effects_handler_impl::slotTabRemoved(EffectWindow* w, EffectWindow* leaderOfFormerGroup)
{
    Q_EMIT tabRemoved(w, leaderOfFormerGroup);
}

void effects_handler_impl::slotWindowDamaged(Toplevel* t, const QRegion& r)
{
    assert(t->render);
    assert(t->render->effect);
    Q_EMIT windowDamaged(t->render->effect.get(), r);
}

void effects_handler_impl::slotGeometryShapeChanged(Toplevel* t, const QRect& old)
{
    assert(t);
    assert(t->render);
    assert(t->render->effect);

    if (t->control && (win::is_move(t) || win::is_resize(t))) {
        // For that we have windowStepUserMovedResized.
        return;
    }

    Q_EMIT windowGeometryShapeChanged(t->render->effect.get(), old);
}

void effects_handler_impl::slotFrameGeometryChanged(Toplevel* toplevel, const QRect& oldGeometry)
{
    assert(toplevel->render);
    assert(toplevel->render->effect);
    Q_EMIT windowFrameGeometryChanged(toplevel->render->effect.get(), oldGeometry);
}

void effects_handler_impl::slotPaddingChanged(Toplevel* t, const QRect& old)
{
    assert(t);
    assert(t->render);
    assert(t->render->effect);
    Q_EMIT windowPaddingChanged(t->render->effect.get(), old);
}

void effects_handler_impl::setActiveFullScreenEffect(Effect* e)
{
    if (fullscreen_effect == e) {
        return;
    }
    const bool activeChanged = (e == nullptr || fullscreen_effect == nullptr);
    fullscreen_effect = e;
    Q_EMIT activeFullScreenEffectChanged();
    if (activeChanged) {
        Q_EMIT hasActiveFullScreenEffectChanged();
        m_compositor->space->edges->checkBlocking();
    }
}

Effect* effects_handler_impl::activeFullScreenEffect() const
{
    return fullscreen_effect;
}

bool effects_handler_impl::hasActiveFullScreenEffect() const
{
    return fullscreen_effect;
}

bool effects_handler_impl::grabKeyboard(Effect* effect)
{
    if (keyboard_grab_effect != nullptr)
        return false;
    if (!doGrabKeyboard()) {
        return false;
    }
    keyboard_grab_effect = effect;
    return true;
}

bool effects_handler_impl::doGrabKeyboard()
{
    return true;
}

void effects_handler_impl::ungrabKeyboard()
{
    Q_ASSERT(keyboard_grab_effect != nullptr);
    doUngrabKeyboard();
    keyboard_grab_effect = nullptr;
}

void effects_handler_impl::doUngrabKeyboard()
{
}

void effects_handler_impl::grabbedKeyboardEvent(QKeyEvent* e)
{
    if (keyboard_grab_effect != nullptr)
        keyboard_grab_effect->grabbedKeyboardEvent(e);
}

void effects_handler_impl::startMouseInterception(Effect* effect, Qt::CursorShape shape)
{
    if (m_grabbedMouseEffects.contains(effect)) {
        return;
    }
    m_grabbedMouseEffects.append(effect);
    if (m_grabbedMouseEffects.size() != 1) {
        return;
    }
    doStartMouseInterception(shape);
}

void effects_handler_impl::doStartMouseInterception(Qt::CursorShape shape)
{
    kwinApp()->input->redirect->get_pointer()->setEffectsOverrideCursor(shape);
}

void effects_handler_impl::stopMouseInterception(Effect* effect)
{
    if (!m_grabbedMouseEffects.contains(effect)) {
        return;
    }
    m_grabbedMouseEffects.removeAll(effect);
    if (m_grabbedMouseEffects.isEmpty()) {
        doStopMouseInterception();
    }
}

void effects_handler_impl::doStopMouseInterception()
{
    kwinApp()->input->redirect->get_pointer()->removeEffectsOverrideCursor();
}

bool effects_handler_impl::isMouseInterception() const
{
    return m_grabbedMouseEffects.count() > 0;
}

bool effects_handler_impl::touchDown(qint32 id, const QPointF& pos, quint32 time)
{
    // TODO: reverse call order?
    for (auto it = loaded_effects.constBegin(); it != loaded_effects.constEnd(); ++it) {
        if (it->second->touchDown(id, pos, time)) {
            return true;
        }
    }
    return false;
}

bool effects_handler_impl::touchMotion(qint32 id, const QPointF& pos, quint32 time)
{
    // TODO: reverse call order?
    for (auto it = loaded_effects.constBegin(); it != loaded_effects.constEnd(); ++it) {
        if (it->second->touchMotion(id, pos, time)) {
            return true;
        }
    }
    return false;
}

bool effects_handler_impl::touchUp(qint32 id, quint32 time)
{
    // TODO: reverse call order?
    for (auto it = loaded_effects.constBegin(); it != loaded_effects.constEnd(); ++it) {
        if (it->second->touchUp(id, time)) {
            return true;
        }
    }
    return false;
}

void effects_handler_impl::registerGlobalShortcut(const QKeySequence& shortcut, QAction* action)
{
    kwinApp()->input->registerShortcut(shortcut, action);
}

void effects_handler_impl::registerPointerShortcut(Qt::KeyboardModifiers modifiers,
                                                   Qt::MouseButton pointerButtons,
                                                   QAction* action)
{
    kwinApp()->input->registerPointerShortcut(modifiers, pointerButtons, action);
}

void effects_handler_impl::registerAxisShortcut(Qt::KeyboardModifiers modifiers,
                                                PointerAxisDirection axis,
                                                QAction* action)
{
    kwinApp()->input->registerAxisShortcut(modifiers, axis, action);
}

void effects_handler_impl::registerTouchpadSwipeShortcut(SwipeDirection direction, QAction* action)
{
    kwinApp()->input->registerTouchpadSwipeShortcut(direction, action);
}

void* effects_handler_impl::getProxy(QString name)
{
    for (QVector<EffectPair>::const_iterator it = loaded_effects.constBegin();
         it != loaded_effects.constEnd();
         ++it)
        if ((*it).first == name)
            return (*it).second->proxy();

    return nullptr;
}

void effects_handler_impl::startMousePolling()
{
    if (auto& cursor = m_compositor->space->input->platform.cursor) {
        cursor->start_mouse_polling();
    }
}

void effects_handler_impl::stopMousePolling()
{
    if (auto& cursor = m_compositor->space->input->platform.cursor) {
        cursor->stop_mouse_polling();
    }
}

bool effects_handler_impl::hasKeyboardGrab() const
{
    return keyboard_grab_effect != nullptr;
}

void effects_handler_impl::desktopResized(const QSize& size)
{
    m_scene->handle_screen_geometry_change(size);
    Q_EMIT screenGeometryChanged(size);
}

xcb_atom_t effects_handler_impl::announceSupportProperty(const QByteArray& propertyName,
                                                         Effect* effect)
{
    return x11::announce_support_property(*this, effect, propertyName);
}

void effects_handler_impl::removeSupportProperty(const QByteArray& propertyName, Effect* effect)
{
    x11::remove_support_property(*this, effect, propertyName);
}

QByteArray effects_handler_impl::readRootProperty(long atom, long type, int format) const
{
    if (!kwinApp()->x11Connection()) {
        return QByteArray();
    }
    return readWindowProperty(kwinApp()->x11RootWindow(), atom, type, format);
}

void effects_handler_impl::activateWindow(EffectWindow* c)
{
    auto window = static_cast<effects_window_impl*>(c)->window();
    if (window && window->control) {
        win::force_activate_window(*m_compositor->space, window);
    }
}

EffectWindow* effects_handler_impl::activeWindow() const
{
    auto ac = m_compositor->space->active_client;
    return ac ? ac->render->effect.get() : nullptr;
}

void effects_handler_impl::moveWindow(EffectWindow* w,
                                      const QPoint& pos,
                                      bool snap,
                                      double snapAdjust)
{
    auto window = static_cast<effects_window_impl*>(w)->window();
    if (!window || !window->isMovable()) {
        return;
    }

    if (snap) {
        win::move(
            window,
            win::adjust_window_position(*m_compositor->space, *window, pos, true, snapAdjust));
    } else {
        win::move(window, pos);
    }
}

void effects_handler_impl::windowToDesktop(EffectWindow* w, int desktop)
{
    auto window = static_cast<effects_window_impl*>(w)->window();
    if (window && window->control && !win::is_desktop(window) && !win::is_dock(window)) {
        win::send_window_to_desktop(*m_compositor->space, window, desktop, true);
    }
}

void effects_handler_impl::windowToDesktops(EffectWindow* w, const QVector<uint>& desktopIds)
{
    auto window = static_cast<effects_window_impl*>(w)->window();
    if (!window || !window->control || win::is_desktop(window) || win::is_dock(window)) {
        return;
    }
    QVector<win::virtual_desktop*> desktops;
    desktops.reserve(desktopIds.count());
    for (uint x11Id : desktopIds) {
        if (x11Id > m_compositor->space->virtual_desktop_manager->count()) {
            continue;
        }
        auto d = m_compositor->space->virtual_desktop_manager->desktopForX11Id(x11Id);
        Q_ASSERT(d);
        if (desktops.contains(d)) {
            continue;
        }
        desktops << d;
    }
    win::set_desktops(window, desktops);
}

void effects_handler_impl::windowToScreen(EffectWindow* w, int screen)
{
    auto output = base::get_output(kwinApp()->get_base().get_outputs(), screen);
    auto window = static_cast<effects_window_impl*>(w)->window();

    if (output && window && window->control && !win::is_desktop(window) && !win::is_dock(window)) {
        win::send_to_screen(*m_compositor->space, window, *output);
    }
}

void effects_handler_impl::setShowingDesktop(bool showing)
{
    win::set_showing_desktop(*m_compositor->space, showing);
}

QString effects_handler_impl::currentActivity() const
{
    return QString();
}

int effects_handler_impl::currentDesktop() const
{
    return m_compositor->space->virtual_desktop_manager->current();
}

int effects_handler_impl::numberOfDesktops() const
{
    return m_compositor->space->virtual_desktop_manager->count();
}

void effects_handler_impl::setCurrentDesktop(int desktop)
{
    m_compositor->space->virtual_desktop_manager->setCurrent(desktop);
}

void effects_handler_impl::setNumberOfDesktops(int desktops)
{
    m_compositor->space->virtual_desktop_manager->setCount(desktops);
}

QSize effects_handler_impl::desktopGridSize() const
{
    return m_compositor->space->virtual_desktop_manager->grid().size();
}

int effects_handler_impl::desktopGridWidth() const
{
    return desktopGridSize().width();
}

int effects_handler_impl::desktopGridHeight() const
{
    return desktopGridSize().height();
}

int effects_handler_impl::workspaceWidth() const
{
    return desktopGridWidth() * kwinApp()->get_base().topology.size.width();
}

int effects_handler_impl::workspaceHeight() const
{
    return desktopGridHeight() * kwinApp()->get_base().topology.size.height();
}

int effects_handler_impl::desktopAtCoords(QPoint coords) const
{
    if (auto vd = m_compositor->space->virtual_desktop_manager->grid().at(coords)) {
        return vd->x11DesktopNumber();
    }
    return 0;
}

QPoint effects_handler_impl::desktopGridCoords(int id) const
{
    return m_compositor->space->virtual_desktop_manager->grid().gridCoords(id);
}

QPoint effects_handler_impl::desktopCoords(int id) const
{
    auto coords = m_compositor->space->virtual_desktop_manager->grid().gridCoords(id);
    if (coords.x() == -1) {
        return QPoint(-1, -1);
    }
    auto const& space_size = kwinApp()->get_base().topology.size;
    return QPoint(coords.x() * space_size.width(), coords.y() * space_size.height());
}

int effects_handler_impl::desktopAbove(int desktop, bool wrap) const
{
    return win::getDesktop<win::virtual_desktop_above>(
        *m_compositor->space->virtual_desktop_manager, desktop, wrap);
}

int effects_handler_impl::desktopToRight(int desktop, bool wrap) const
{
    return win::getDesktop<win::virtual_desktop_right>(
        *m_compositor->space->virtual_desktop_manager, desktop, wrap);
}

int effects_handler_impl::desktopBelow(int desktop, bool wrap) const
{
    return win::getDesktop<win::virtual_desktop_below>(
        *m_compositor->space->virtual_desktop_manager, desktop, wrap);
}

int effects_handler_impl::desktopToLeft(int desktop, bool wrap) const
{
    return win::getDesktop<win::virtual_desktop_left>(
        *m_compositor->space->virtual_desktop_manager, desktop, wrap);
}

QString effects_handler_impl::desktopName(int desktop) const
{
    return m_compositor->space->virtual_desktop_manager->name(desktop);
}

bool effects_handler_impl::optionRollOverDesktops() const
{
    return kwinApp()->options->isRollOverDesktops();
}

double effects_handler_impl::animationTimeFactor() const
{
    return kwinApp()->options->animationTimeFactor();
}

WindowQuadType effects_handler_impl::newWindowQuadType()
{
    return WindowQuadType(next_window_quad_type++);
}

EffectWindow* effects_handler_impl::find_window_by_wid(WId id) const
{
    if (auto w = win::x11::find_controlled_window<win::x11::window>(
            *m_compositor->space, win::x11::predicate_match::window, id)) {
        return w->render->effect.get();
    }
    if (auto unmanaged = win::x11::find_unmanaged<win::x11::window>(*m_compositor->space, id)) {
        return unmanaged->render->effect.get();
    }
    return nullptr;
}

EffectWindow*
effects_handler_impl::find_window_by_surface(Wrapland::Server::Surface* /*surface*/) const
{
    return nullptr;
}

EffectWindow* effects_handler_impl::find_window_by_qwindow(QWindow* w) const
{
    if (Toplevel* toplevel = m_compositor->space->findInternal(w)) {
        return toplevel->render->effect.get();
    }
    return nullptr;
}

EffectWindow* effects_handler_impl::find_window_by_uuid(const QUuid& id) const
{
    for (auto win : m_compositor->space->windows) {
        if (!win->remnant && win->internal_id == id) {
            return win->render->effect.get();
        }
    }
    return nullptr;
}

EffectWindowList effects_handler_impl::stackingOrder() const
{
    auto list = win::render_stack(*m_compositor->space->stacking_order);
    EffectWindowList ret;
    for (auto t : list) {
        if (EffectWindow* w = effectWindow(t))
            ret.append(w);
    }
    return ret;
}

void effects_handler_impl::setElevatedWindow(KWin::EffectWindow* w, bool set)
{
    elevated_windows.removeAll(w);
    if (set)
        elevated_windows.append(w);
}

void effects_handler_impl::setTabBoxWindow(EffectWindow* w)
{
#if KWIN_BUILD_TABBOX
    auto window = static_cast<effects_window_impl*>(w)->window();
    if (window->control) {
        m_compositor->space->tabbox->set_current_client(window);
    }
#else
    Q_UNUSED(w)
#endif
}

void effects_handler_impl::setTabBoxDesktop(int desktop)
{
#if KWIN_BUILD_TABBOX
    m_compositor->space->tabbox->set_current_desktop(desktop);
#else
    Q_UNUSED(desktop)
#endif
}

EffectWindowList effects_handler_impl::currentTabBoxWindowList() const
{
#if KWIN_BUILD_TABBOX
    const auto clients = m_compositor->space->tabbox->current_client_list();
    EffectWindowList ret;
    ret.reserve(clients.size());
    std::transform(std::cbegin(clients),
                   std::cend(clients),
                   std::back_inserter(ret),
                   [](auto client) { return client->render->effect.get(); });
    return ret;
#else
    return EffectWindowList();
#endif
}

void effects_handler_impl::refTabBox()
{
#if KWIN_BUILD_TABBOX
    m_compositor->space->tabbox->reference();
#endif
}

void effects_handler_impl::unrefTabBox()
{
#if KWIN_BUILD_TABBOX
    m_compositor->space->tabbox->unreference();
#endif
}

void effects_handler_impl::closeTabBox()
{
#if KWIN_BUILD_TABBOX
    m_compositor->space->tabbox->close();
#endif
}

QList<int> effects_handler_impl::currentTabBoxDesktopList() const
{
#if KWIN_BUILD_TABBOX
    return m_compositor->space->tabbox->current_desktop_list();
#else
    return QList<int>();
#endif
}

int effects_handler_impl::currentTabBoxDesktop() const
{
#if KWIN_BUILD_TABBOX
    return m_compositor->space->tabbox->current_desktop();
#else
    return -1;
#endif
}

EffectWindow* effects_handler_impl::currentTabBoxWindow() const
{
#if KWIN_BUILD_TABBOX
    if (auto c = m_compositor->space->tabbox->current_client())
        return c->render->effect.get();
#endif
    return nullptr;
}

void effects_handler_impl::addRepaintFull()
{
    m_compositor->addRepaintFull();
}

void effects_handler_impl::addRepaint(const QRect& r)
{
    m_compositor->addRepaint(r);
}

void effects_handler_impl::addRepaint(const QRegion& r)
{
    m_compositor->addRepaint(r);
}

void effects_handler_impl::addRepaint(int x, int y, int w, int h)
{
    m_compositor->addRepaint(QRegion(x, y, w, h));
}

int effects_handler_impl::activeScreen() const
{
    auto output = win::get_current_output(*m_compositor->space);
    if (!output) {
        return 0;
    }
    return base::get_output_index(kwinApp()->get_base().get_outputs(), *output);
}

int effects_handler_impl::numScreens() const
{
    return kwinApp()->get_base().get_outputs().size();
}

int effects_handler_impl::screenNumber(const QPoint& pos) const
{
    auto const& outputs = kwinApp()->get_base().get_outputs();
    auto output = base::get_nearest_output(outputs, pos);
    if (!output) {
        return 0;
    }
    return base::get_output_index(outputs, *output);
}

QRect effects_handler_impl::clientArea(clientAreaOption opt, int screen, int desktop) const
{
    auto output = base::get_output(kwinApp()->get_base().get_outputs(), screen);
    return win::space_window_area(*m_compositor->space, opt, output, desktop);
}

QRect effects_handler_impl::clientArea(clientAreaOption opt, const EffectWindow* c) const
{
    auto window = static_cast<effects_window_impl const*>(c)->window();
    auto space = m_compositor->space;

    if (window->control) {
        return win::space_window_area(*space, opt, window);
    } else {
        return win::space_window_area(*space,
                                      opt,
                                      window->frameGeometry().center(),
                                      space->virtual_desktop_manager->current());
    }
}

QRect effects_handler_impl::clientArea(clientAreaOption opt, const QPoint& p, int desktop) const
{
    return win::space_window_area(*m_compositor->space, opt, p, desktop);
}

QRect effects_handler_impl::virtualScreenGeometry() const
{
    return QRect({}, kwinApp()->get_base().topology.size);
}

QSize effects_handler_impl::virtualScreenSize() const
{
    return kwinApp()->get_base().topology.size;
}

void effects_handler_impl::defineCursor(Qt::CursorShape shape)
{
    kwinApp()->input->redirect->get_pointer()->setEffectsOverrideCursor(shape);
}

bool effects_handler_impl::checkInputWindowEvent(QMouseEvent* e)
{
    if (m_grabbedMouseEffects.isEmpty()) {
        return false;
    }
    for (auto const& effect : qAsConst(m_grabbedMouseEffects)) {
        effect->windowInputMouseEvent(e);
    }
    return true;
}

bool effects_handler_impl::checkInputWindowEvent(QWheelEvent* e)
{
    if (m_grabbedMouseEffects.isEmpty()) {
        return false;
    }
    for (auto const& effect : qAsConst(m_grabbedMouseEffects)) {
        effect->windowInputMouseEvent(e);
    }
    return true;
}

void effects_handler_impl::connectNotify(const QMetaMethod& signal)
{
    if (signal == QMetaMethod::fromSignal(&EffectsHandler::cursorShapeChanged)) {
        if (!m_trackingCursorChanges) {
            QObject::connect(m_compositor->space->input->platform.cursor.get(),
                             &input::cursor::image_changed,
                             this,
                             &EffectsHandler::cursorShapeChanged);
            m_compositor->space->input->platform.cursor->start_image_tracking();
        }
        ++m_trackingCursorChanges;
    }
    EffectsHandler::connectNotify(signal);
}

void effects_handler_impl::disconnectNotify(const QMetaMethod& signal)
{
    if (signal == QMetaMethod::fromSignal(&EffectsHandler::cursorShapeChanged)) {
        Q_ASSERT(m_trackingCursorChanges > 0);
        if (!--m_trackingCursorChanges) {
            m_compositor->space->input->platform.cursor->stop_image_tracking();
            QObject::disconnect(m_compositor->space->input->platform.cursor.get(),
                                &input::cursor::image_changed,
                                this,
                                &EffectsHandler::cursorShapeChanged);
        }
    }
    EffectsHandler::disconnectNotify(signal);
}

void effects_handler_impl::checkInputWindowStacking()
{
    if (m_grabbedMouseEffects.isEmpty()) {
        return;
    }
    doCheckInputWindowStacking();
}

void effects_handler_impl::doCheckInputWindowStacking()
{
}

QPoint effects_handler_impl::cursorPos() const
{
    return m_compositor->space->input->platform.cursor->pos();
}

void effects_handler_impl::reserveElectricBorder(ElectricBorder border, Effect* effect)
{
    m_compositor->space->edges->reserve(border, effect, "borderActivated");
}

void effects_handler_impl::unreserveElectricBorder(ElectricBorder border, Effect* effect)
{
    m_compositor->space->edges->unreserve(border, effect);
}

void effects_handler_impl::registerTouchBorder(ElectricBorder border, QAction* action)
{
    m_compositor->space->edges->reserveTouch(border, action);
}

void effects_handler_impl::unregisterTouchBorder(ElectricBorder border, QAction* action)
{
    m_compositor->space->edges->unreserveTouch(border, action);
}

unsigned long effects_handler_impl::xrenderBufferPicture() const
{
    return m_scene->xrenderBufferPicture();
}

QPainter* effects_handler_impl::scenePainter()
{
    return m_scene->scenePainter();
}

void effects_handler_impl::toggleEffect(const QString& name)
{
    if (isEffectLoaded(name))
        unloadEffect(name);
    else
        loadEffect(name);
}

QStringList effects_handler_impl::loadedEffects() const
{
    QStringList listModules;
    listModules.reserve(loaded_effects.count());
    std::transform(loaded_effects.constBegin(),
                   loaded_effects.constEnd(),
                   std::back_inserter(listModules),
                   [](const EffectPair& pair) { return pair.first; });
    return listModules;
}

QStringList effects_handler_impl::listOfEffects() const
{
    return m_effectLoader->listOfKnownEffects();
}

bool effects_handler_impl::loadEffect(const QString& name)
{
    makeOpenGLContextCurrent();
    m_compositor->addRepaintFull();

    return m_effectLoader->loadEffect(name);
}

void effects_handler_impl::unloadEffect(const QString& name)
{
    auto it = std::find_if(effect_order.begin(), effect_order.end(), [name](EffectPair& pair) {
        return pair.first == name;
    });
    if (it == effect_order.end()) {
        qCDebug(KWIN_CORE) << "EffectsHandler::unloadEffect : Effect not loaded :" << name;
        return;
    }

    qCDebug(KWIN_CORE) << "EffectsHandler::unloadEffect : Unloading Effect :" << name;
    destroyEffect((*it).second);
    effect_order.erase(it);
    effectsChanged();

    m_compositor->addRepaintFull();
}

void effects_handler_impl::handle_effect_destroy(Effect& /*effect*/)
{
}

void effects_handler_impl::destroyEffect(Effect* effect)
{
    assert(effect);
    makeOpenGLContextCurrent();

    if (fullscreen_effect == effect) {
        setActiveFullScreenEffect(nullptr);
    }

    if (keyboard_grab_effect == effect) {
        ungrabKeyboard();
    }

    stopMouseInterception(effect);
    handle_effect_destroy(*effect);

    const QList<QByteArray> properties = m_propertiesForEffects.keys();
    for (const QByteArray& property : properties) {
        removeSupportProperty(property, effect);
    }

    delete effect;
}

void effects_handler_impl::reconfigureEffect(const QString& name)
{
    for (QVector<EffectPair>::const_iterator it = loaded_effects.constBegin();
         it != loaded_effects.constEnd();
         ++it)
        if ((*it).first == name) {
            kwinApp()->config()->reparseConfiguration();
            makeOpenGLContextCurrent();
            (*it).second->reconfigure(Effect::ReconfigureAll);
            return;
        }
}

bool effects_handler_impl::isEffectLoaded(const QString& name) const
{
    auto it = std::find_if(loaded_effects.constBegin(),
                           loaded_effects.constEnd(),
                           [&name](const EffectPair& pair) { return pair.first == name; });
    return it != loaded_effects.constEnd();
}

bool effects_handler_impl::isEffectSupported(const QString& name)
{
    // If the effect is loaded, it is obviously supported.
    if (isEffectLoaded(name)) {
        return true;
    }

    // next checks might require a context
    makeOpenGLContextCurrent();

    return m_effectLoader->isEffectSupported(name);
}

QList<bool> effects_handler_impl::areEffectsSupported(const QStringList& names)
{
    QList<bool> retList;
    retList.reserve(names.count());
    std::transform(names.constBegin(),
                   names.constEnd(),
                   std::back_inserter(retList),
                   [this](const QString& name) { return isEffectSupported(name); });
    return retList;
}

void effects_handler_impl::reloadEffect(Effect* effect)
{
    QString effectName;
    for (QVector<EffectPair>::const_iterator it = loaded_effects.constBegin();
         it != loaded_effects.constEnd();
         ++it) {
        if ((*it).second == effect) {
            effectName = (*it).first;
            break;
        }
    }
    if (!effectName.isNull()) {
        unloadEffect(effectName);
        m_effectLoader->loadEffect(effectName);
    }
}

void effects_handler_impl::effectsChanged()
{
    loaded_effects.clear();
    m_activeEffects.clear(); // it's possible to have a reconfigure and a quad rebuild between two
                             // paint cycles - bug #308201

    loaded_effects.reserve(effect_order.count());
    std::copy(
        effect_order.constBegin(), effect_order.constEnd(), std::back_inserter(loaded_effects));

    m_activeEffects.reserve(loaded_effects.count());
}

QStringList effects_handler_impl::activeEffects() const
{
    QStringList ret;
    for (QVector<KWin::EffectPair>::const_iterator it = loaded_effects.constBegin(),
                                                   end = loaded_effects.constEnd();
         it != end;
         ++it) {
        if (it->second->isActive()) {
            ret << it->first;
        }
    }
    return ret;
}

Wrapland::Server::Display* effects_handler_impl::waylandDisplay() const
{
    return nullptr;
}

EffectFrame* effects_handler_impl::effectFrame(EffectFrameStyle style,
                                               bool staticSize,
                                               const QPoint& position,
                                               Qt::Alignment alignment) const
{
    return new effect_frame_impl(*m_scene, style, staticSize, position, alignment);
}

QVariant effects_handler_impl::kwinOption(KWinOption kwopt)
{
    switch (kwopt) {
    case CloseButtonCorner: {
        // TODO: this could become per window and be derived from the actual position in the deco
        auto deco_settings = m_compositor->space->deco->settings();
        auto close_enum = KDecoration2::DecorationButtonType::Close;
        return deco_settings && deco_settings->decorationButtonsLeft().contains(close_enum)
            ? Qt::TopLeftCorner
            : Qt::TopRightCorner;
    }
    case SwitchDesktopOnScreenEdge:
        return m_compositor->space->edges->desktop_switching.always;
    case SwitchDesktopOnScreenEdgeMovingWindows:
        return m_compositor->space->edges->desktop_switching.when_moving_client;
    default:
        return QVariant(); // an invalid one
    }
}

QString effects_handler_impl::supportInformation(const QString& name) const
{
    auto it = std::find_if(loaded_effects.constBegin(),
                           loaded_effects.constEnd(),
                           [name](const EffectPair& pair) { return pair.first == name; });
    if (it == loaded_effects.constEnd()) {
        return QString();
    }

    QString support((*it).first + QLatin1String(":\n"));
    const QMetaObject* metaOptions = (*it).second->metaObject();
    for (int i = 0; i < metaOptions->propertyCount(); ++i) {
        const QMetaProperty property = metaOptions->property(i);
        if (qstrcmp(property.name(), "objectName") == 0) {
            continue;
        }
        support += QString::fromUtf8(property.name()) + QLatin1String(": ")
            + (*it).second->property(property.name()).toString() + QLatin1Char('\n');
    }

    return support;
}

bool effects_handler_impl::isScreenLocked() const
{
    return kwinApp()->screen_locker_watcher->is_locked();
}

QString effects_handler_impl::debug(const QString& name, const QString& parameter) const
{
    QString internalName = name.toLower();
    ;
    for (QVector<EffectPair>::const_iterator it = loaded_effects.constBegin();
         it != loaded_effects.constEnd();
         ++it) {
        if ((*it).first == internalName) {
            return it->second->debug(parameter);
        }
    }
    return QString();
}

bool effects_handler_impl::makeOpenGLContextCurrent()
{
    return m_scene->makeOpenGLContextCurrent();
}

void effects_handler_impl::doneOpenGLContextCurrent()
{
    m_scene->doneOpenGLContextCurrent();
}

bool effects_handler_impl::animationsSupported() const
{
    static const QByteArray forceEnvVar = qgetenv("KWIN_EFFECTS_FORCE_ANIMATIONS");
    if (!forceEnvVar.isEmpty()) {
        static const int forceValue = forceEnvVar.toInt();
        return forceValue == 1;
    }
    return m_scene->animationsSupported();
}

void effects_handler_impl::highlightWindows(const QVector<EffectWindow*>& windows)
{
    Effect* e = provides(Effect::HighlightWindows);
    if (!e) {
        return;
    }
    e->perform(Effect::HighlightWindows, QVariantList{QVariant::fromValue(windows)});
}

PlatformCursorImage effects_handler_impl::cursorImage() const
{
    return kwinApp()->input->cursor->platform_image();
}

void effects_handler_impl::hideCursor()
{
    kwinApp()->input->cursor->hide();
}

void effects_handler_impl::showCursor()
{
    kwinApp()->input->cursor->show();
}

void effects_handler_impl::startInteractiveWindowSelection(
    std::function<void(KWin::EffectWindow*)> callback)
{
    kwinApp()->input->start_interactive_window_selection([callback](KWin::Toplevel* t) {
        if (t) {
            assert(t->render);
            assert(t->render->effect);
            callback(t->render->effect.get());
        } else {
            callback(nullptr);
        }
    });
}

void effects_handler_impl::startInteractivePositionSelection(
    std::function<void(const QPoint&)> callback)
{
    kwinApp()->input->start_interactive_position_selection(callback);
}

void effects_handler_impl::showOnScreenMessage(const QString& message, const QString& iconName)
{
    win::osd_show(*m_compositor->space, message, iconName);
}

void effects_handler_impl::hideOnScreenMessage(OnScreenMessageHideFlags flags)
{
    win::osd_hide_flags internal_flags{};
    if (flags.testFlag(OnScreenMessageHideFlag::SkipsCloseAnimation)) {
        internal_flags |= win::osd_hide_flags::skip_close_animation;
    }
    win::osd_hide(*m_compositor->space, internal_flags);
}

KSharedConfigPtr effects_handler_impl::config() const
{
    return kwinApp()->config();
}

KSharedConfigPtr effects_handler_impl::inputConfig() const
{
    return kwinApp()->inputConfig();
}

Effect* effects_handler_impl::findEffect(const QString& name) const
{
    auto it = std::find_if(loaded_effects.constBegin(),
                           loaded_effects.constEnd(),
                           [name](const EffectPair& pair) { return pair.first == name; });
    if (it == loaded_effects.constEnd()) {
        return nullptr;
    }
    return (*it).second;
}

void effects_handler_impl::renderEffectQuickView(EffectQuickView* w) const
{
    if (!w->isVisible()) {
        return;
    }
    scene()->paintEffectQuickView(w);
}

SessionState effects_handler_impl::sessionState() const
{
    return m_compositor->space->session_manager->state();
}

QList<EffectScreen*> effects_handler_impl::screens() const
{
    return m_effectScreens;
}

EffectScreen* effects_handler_impl::screenAt(const QPoint& point) const
{
    return m_effectScreens.value(screenNumber(point));
}

EffectScreen* effects_handler_impl::findScreen(const QString& name) const
{
    for (EffectScreen* screen : qAsConst(m_effectScreens)) {
        if (screen->name() == name) {
            return screen;
        }
    }
    return nullptr;
}

EffectScreen* effects_handler_impl::findScreen(int screenId) const
{
    return m_effectScreens.value(screenId);
}

void effects_handler_impl::slotOutputEnabled(base::output* output)
{
    EffectScreen* screen = new effect_screen_impl(output, this);
    m_effectScreens.append(screen);
    Q_EMIT screenAdded(screen);
}

void effects_handler_impl::slotOutputDisabled(base::output* output)
{
    auto it = std::find_if(
        m_effectScreens.begin(), m_effectScreens.end(), [&output](EffectScreen* screen) {
            return static_cast<effect_screen_impl*>(screen)->platformOutput() == output;
        });
    if (it != m_effectScreens.end()) {
        EffectScreen* screen = *it;
        m_effectScreens.erase(it);
        Q_EMIT screenRemoved(screen);
        delete screen;
    }
}

bool effects_handler_impl::isCursorHidden() const
{
    return m_compositor->space->input->platform.cursor->is_hidden();
}

QImage effects_handler_impl::blit_from_framebuffer(QRect const& geometry, double scale) const
{
    if (!isOpenGLCompositing()) {
        return {};
    }

    QImage image;
    auto const nativeSize = geometry.size() * scale;

    if (GLRenderTarget::blitSupported() && !GLPlatform::instance()->isGLES()) {
        image = QImage(nativeSize.width(), nativeSize.height(), QImage::Format_ARGB32);

        GLTexture texture(GL_RGBA8, nativeSize.width(), nativeSize.height());
        GLRenderTarget target(texture);
        target.blitFromFramebuffer(mapToRenderTarget(geometry));

        // Copy content from framebuffer into image.
        texture.bind();
        glGetTexImage(
            GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, static_cast<GLvoid*>(image.bits()));
        texture.unbind();
    } else {
        image = QImage(nativeSize.width(), nativeSize.height(), QImage::Format_ARGB32);
        glReadPixels(0,
                     0,
                     nativeSize.width(),
                     nativeSize.height(),
                     GL_RGBA,
                     GL_UNSIGNED_BYTE,
                     static_cast<GLvoid*>(image.bits()));
    }

    auto gl_backend = static_cast<render::gl::scene*>(m_scene)->backend();
    QMatrix4x4 flip_vert;
    flip_vert(1, 1) = -1;

    image = image.transformed((flip_vert * gl_backend->transformation).toTransform());
    image.setDevicePixelRatio(scale);
    return image;
}

bool effects_handler_impl::invert_screen()
{
    if (auto inverter = provides(Effect::ScreenInversion)) {
        qCDebug(KWIN_CORE) << "inverting screen using Effect plugin";
        QMetaObject::invokeMethod(inverter, "toggleScreenInversion", Qt::DirectConnection);
        return true;
    }
    return false;
}

QRect effects_handler_impl::renderTargetRect() const
{
    return m_scene->renderTargetRect();
}

qreal effects_handler_impl::renderTargetScale() const
{
    return m_scene->renderTargetScale();
}

//****************************************
// effect_screen_impl
//****************************************

effect_screen_impl::effect_screen_impl(base::output* output, QObject* parent)
    : EffectScreen(parent)
    , m_platformOutput(output)
{
    connect(output, &base::output::wake_up, this, &EffectScreen::wakeUp);
    connect(output, &base::output::about_to_turn_off, this, &EffectScreen::aboutToTurnOff);
    connect(output, &base::output::scale_changed, this, &EffectScreen::devicePixelRatioChanged);
    connect(output, &base::output::geometry_changed, this, &EffectScreen::geometryChanged);
}

base::output* effect_screen_impl::platformOutput() const
{
    return m_platformOutput;
}

QString effect_screen_impl::name() const
{
    return m_platformOutput->name();
}

qreal effect_screen_impl::devicePixelRatio() const
{
    return m_platformOutput->scale();
}

QRect effect_screen_impl::geometry() const
{
    return m_platformOutput->geometry();
}

//****************************************
// effects_window_impl
//****************************************

effects_window_impl::effects_window_impl(Toplevel* toplevel)
    : EffectWindow(toplevel)
    , toplevel(toplevel)
    , sw(nullptr)
{
    // Deleted windows are not managed. So, when windowClosed signal is
    // emitted, effects can't distinguish managed windows from unmanaged
    // windows(e.g. combo box popups, popup menus, etc). Save value of the
    // managed property during construction of EffectWindow. At that time,
    // parent can be Client, XdgShellClient, or Unmanaged. So, later on, when
    // an instance of Deleted becomes parent of the EffectWindow, effects
    // can still figure out whether it is/was a managed window.
    managed = toplevel->isClient();

    waylandClient = toplevel->is_wayland_window();
    x11Client = qobject_cast<KWin::win::x11::window*>(toplevel) != nullptr || toplevel->xcb_window;
}

effects_window_impl::~effects_window_impl()
{
    QVariant cachedTextureVariant = data(LanczosCacheRole);
    if (cachedTextureVariant.isValid()) {
        GLTexture* cachedTexture = static_cast<GLTexture*>(cachedTextureVariant.value<void*>());
        delete cachedTexture;
    }
}

bool effects_window_impl::isPaintingEnabled()
{
    return sceneWindow()->isPaintingEnabled();
}

void effects_window_impl::enablePainting(int reason)
{
    sceneWindow()->enablePainting(static_cast<render::window_paint_disable_type>(reason));
}

void effects_window_impl::disablePainting(int reason)
{
    sceneWindow()->disablePainting(static_cast<render::window_paint_disable_type>(reason));
}

void effects_window_impl::addRepaint(const QRect& r)
{
    toplevel->addRepaint(r);
}

void effects_window_impl::addRepaint(int x, int y, int w, int h)
{
    addRepaint(QRect(x, y, w, h));
}

void effects_window_impl::addRepaintFull()
{
    toplevel->addRepaintFull();
}

void effects_window_impl::addLayerRepaint(const QRect& r)
{
    toplevel->addLayerRepaint(r);
}

void effects_window_impl::addLayerRepaint(int x, int y, int w, int h)
{
    addLayerRepaint(QRect(x, y, w, h));
}

const EffectWindowGroup* effects_window_impl::group() const
{
    if (auto c = qobject_cast<win::x11::window*>(toplevel); c && c->group()) {
        return c->group()->effect_group;
    }
    return nullptr; // TODO
}

void effects_window_impl::refWindow()
{
    if (toplevel->transient()->annexed) {
        return;
    }
    if (auto& remnant = toplevel->remnant) {
        return remnant->ref();
    }
    abort(); // TODO
}

void effects_window_impl::unrefWindow()
{
    if (toplevel->transient()->annexed) {
        return;
    }
    if (auto& remnant = toplevel->remnant) {
        // delays deletion in case
        return remnant->unref();
    }
    abort(); // TODO
}

QRect effects_window_impl::rect() const
{
    return QRect(QPoint(), toplevel->size());
}

#define TOPLEVEL_HELPER(rettype, prototype, toplevelPrototype)                                     \
    rettype effects_window_impl::prototype() const                                                 \
    {                                                                                              \
        return toplevel->toplevelPrototype();                                                      \
    }

TOPLEVEL_HELPER(double, opacity, opacity)
TOPLEVEL_HELPER(bool, hasAlpha, hasAlpha)
TOPLEVEL_HELPER(int, x, pos().x)
TOPLEVEL_HELPER(int, y, pos().y)
TOPLEVEL_HELPER(int, width, size().width)
TOPLEVEL_HELPER(int, height, size().height)
TOPLEVEL_HELPER(QPoint, pos, pos)
TOPLEVEL_HELPER(QSize, size, size)
TOPLEVEL_HELPER(QRect, geometry, frameGeometry)
TOPLEVEL_HELPER(QRect, frameGeometry, frameGeometry)
TOPLEVEL_HELPER(int, desktop, desktop)
TOPLEVEL_HELPER(QString, windowRole, windowRole)
TOPLEVEL_HELPER(bool, skipsCloseAnimation, skipsCloseAnimation)
TOPLEVEL_HELPER(bool, isOutline, isOutline)
TOPLEVEL_HELPER(bool, isLockScreen, isLockScreen)
TOPLEVEL_HELPER(pid_t, pid, pid)
TOPLEVEL_HELPER(bool, isModal, transient()->modal)

#undef TOPLEVEL_HELPER

#define TOPLEVEL_HELPER_WIN(rettype, prototype, function)                                          \
    rettype effects_window_impl::prototype() const                                                 \
    {                                                                                              \
        return win::function(toplevel);                                                            \
    }

TOPLEVEL_HELPER_WIN(bool, isComboBox, is_combo_box)
TOPLEVEL_HELPER_WIN(bool, isCriticalNotification, is_critical_notification)
TOPLEVEL_HELPER_WIN(bool, isDesktop, is_desktop)
TOPLEVEL_HELPER_WIN(bool, isDialog, is_dialog)
TOPLEVEL_HELPER_WIN(bool, isDNDIcon, is_dnd_icon)
TOPLEVEL_HELPER_WIN(bool, isDock, is_dock)
TOPLEVEL_HELPER_WIN(bool, isDropdownMenu, is_dropdown_menu)
TOPLEVEL_HELPER_WIN(bool, isMenu, is_menu)
TOPLEVEL_HELPER_WIN(bool, isNormalWindow, is_normal)
TOPLEVEL_HELPER_WIN(bool, isNotification, is_notification)
TOPLEVEL_HELPER_WIN(bool, isPopupMenu, is_popup_menu)
TOPLEVEL_HELPER_WIN(bool, isPopupWindow, is_popup)
TOPLEVEL_HELPER_WIN(bool, isOnScreenDisplay, is_on_screen_display)
TOPLEVEL_HELPER_WIN(bool, isSplash, is_splash)
TOPLEVEL_HELPER_WIN(bool, isToolbar, is_toolbar)
TOPLEVEL_HELPER_WIN(bool, isUtility, is_utility)
TOPLEVEL_HELPER_WIN(bool, isTooltip, is_tooltip)
TOPLEVEL_HELPER_WIN(QRect, bufferGeometry, render_geometry)

#undef TOPLEVEL_HELPER_WIN

#define CLIENT_HELPER_WITH_DELETED_WIN(rettype, prototype, propertyname, defaultValue)             \
    rettype effects_window_impl::prototype() const                                                 \
    {                                                                                              \
        if (toplevel->control || toplevel->remnant) {                                              \
            return win::propertyname(toplevel);                                                    \
        }                                                                                          \
        return defaultValue;                                                                       \
    }

CLIENT_HELPER_WITH_DELETED_WIN(QString, caption, caption, QString())
CLIENT_HELPER_WITH_DELETED_WIN(QVector<uint>, desktops, x11_desktop_ids, QVector<uint>())

#undef CLIENT_HELPER_WITH_DELETED_WIN

#define CLIENT_HELPER_WITH_DELETED_WIN_CTRL(rettype, prototype, propertyname, defaultValue)        \
    rettype effects_window_impl::prototype() const                                                 \
    {                                                                                              \
        if (toplevel->control) {                                                                   \
            return toplevel->control->propertyname();                                              \
        }                                                                                          \
        if (auto& remnant = toplevel->remnant) {                                                   \
            return remnant->data.propertyname;                                                     \
        }                                                                                          \
        return defaultValue;                                                                       \
    }

CLIENT_HELPER_WITH_DELETED_WIN_CTRL(bool, keepAbove, keep_above, false)
CLIENT_HELPER_WITH_DELETED_WIN_CTRL(bool, keepBelow, keep_below, false)
CLIENT_HELPER_WITH_DELETED_WIN_CTRL(bool, isMinimized, minimized, false)
CLIENT_HELPER_WITH_DELETED_WIN_CTRL(bool, isFullScreen, fullscreen, false)

#undef CLIENT_HELPER_WITH_DELETED_WIN_CTRL

bool effects_window_impl::isDeleted() const
{
    return static_cast<bool>(toplevel->remnant);
}

Wrapland::Server::Surface* effects_window_impl::surface() const
{
    return toplevel->surface;
}

QStringList effects_window_impl::activities() const
{
    // No support for Activities.
    return {};
}

int effects_window_impl::screen() const
{
    if (!toplevel->central_output) {
        return 0;
    }
    return base::get_output_index(kwinApp()->get_base().get_outputs(), *toplevel->central_output);
}

QRect effects_window_impl::clientGeometry() const
{
    return win::frame_to_client_rect(toplevel, toplevel->frameGeometry());
}

QRect expanded_geometry_recursion(Toplevel* window)
{
    QRect geo;
    for (auto child : window->transient()->children) {
        if (child->transient()->annexed) {
            geo |= expanded_geometry_recursion(child);
        }
    }
    return geo |= win::visible_rect(window);
}

QRect effects_window_impl::expandedGeometry() const
{
    return expanded_geometry_recursion(toplevel);
}

// legacy from tab groups, can be removed when no effects use this any more.
bool effects_window_impl::isCurrentTab() const
{
    return true;
}

QString effects_window_impl::windowClass() const
{
    return toplevel->resource_name + QLatin1Char(' ') + toplevel->resource_class;
}

QRect effects_window_impl::contentsRect() const
{
    // TODO(romangg): This feels kind of wrong. Why are the frame extents not part of it (i.e. just
    //                using frame_to_client_rect)? But some clients rely on the current version,
    //                for example Latte for its behind-dock blur.
    auto const deco_offset = QPoint(win::left_border(toplevel), win::top_border(toplevel));
    auto const client_size = win::frame_relative_client_rect(toplevel).size();

    return QRect(deco_offset, client_size);
}

NET::WindowType effects_window_impl::windowType() const
{
    return toplevel->windowType();
}

#define CLIENT_HELPER(rettype, prototype, propertyname, defaultValue)                              \
    rettype effects_window_impl::prototype() const                                                 \
    {                                                                                              \
        if (toplevel->control) {                                                                   \
            return toplevel->propertyname();                                                       \
        }                                                                                          \
        return defaultValue;                                                                       \
    }

CLIENT_HELPER(bool, isMovable, isMovable, false)
CLIENT_HELPER(bool, isMovableAcrossScreens, isMovableAcrossScreens, false)
CLIENT_HELPER(QRect, iconGeometry, iconGeometry, QRect())
CLIENT_HELPER(bool, acceptsFocus, wantsInput, true) // We don't actually know...

#undef CLIENT_HELPER

#define CLIENT_HELPER_WIN(rettype, prototype, function, default_value)                             \
    rettype effects_window_impl::prototype() const                                                 \
    {                                                                                              \
        if (toplevel->control) {                                                                   \
            return win::function(toplevel);                                                        \
        }                                                                                          \
        return default_value;                                                                      \
    }

CLIENT_HELPER_WIN(bool, isSpecialWindow, is_special_window, true)
CLIENT_HELPER_WIN(bool, isUserMove, is_move, false)
CLIENT_HELPER_WIN(bool, isUserResize, is_resize, false)
CLIENT_HELPER_WIN(bool, decorationHasAlpha, decoration_has_alpha, false)

#undef CLIENT_HELPER_WIN

#define CLIENT_HELPER_WIN_CONTROL(rettype, prototype, function, default_value)                     \
    rettype effects_window_impl::prototype() const                                                 \
    {                                                                                              \
        if (toplevel->control) {                                                                   \
            return toplevel->control->function();                                                  \
        }                                                                                          \
        return default_value;                                                                      \
    }

CLIENT_HELPER_WIN_CONTROL(bool, isSkipSwitcher, skip_switcher, false)
CLIENT_HELPER_WIN_CONTROL(QIcon, icon, icon, QIcon())
CLIENT_HELPER_WIN_CONTROL(bool, isUnresponsive, unresponsive, false)

#undef CLIENT_HELPER_WIN_CONTROL

QSize effects_window_impl::basicUnit() const
{
    if (auto client = qobject_cast<win::x11::window*>(toplevel)) {
        return client->basicUnit();
    }
    return QSize(1, 1);
}

void effects_window_impl::setWindow(Toplevel* w)
{
    toplevel = w;
    setParent(w);
}

void effects_window_impl::setSceneWindow(render::window* w)
{
    sw = w;
}

QRect effects_window_impl::decorationInnerRect() const
{
    return contentsRect();
}

KDecoration2::Decoration* effects_window_impl::decoration() const
{
    return win::decoration(toplevel);
}

QByteArray effects_window_impl::readProperty(long atom, long type, int format) const
{
    if (!kwinApp()->x11Connection()) {
        return QByteArray();
    }
    return readWindowProperty(window()->xcb_window, atom, type, format);
}

void effects_window_impl::deleteProperty(long int atom) const
{
    if (kwinApp()->x11Connection()) {
        deleteWindowProperty(window()->xcb_window, atom);
    }
}

EffectWindow* effects_window_impl::findModal()
{
    if (!toplevel->control) {
        return nullptr;
    }

    auto modal = toplevel->findModal();
    if (modal) {
        return modal->render->effect.get();
    }

    return nullptr;
}

EffectWindow* effects_window_impl::transientFor()
{
    if (!toplevel->control) {
        return nullptr;
    }

    auto transientFor = toplevel->transient()->lead();
    if (transientFor) {
        return transientFor->render->effect.get();
    }

    return nullptr;
}

QWindow* effects_window_impl::internalWindow() const
{
    auto client = qobject_cast<win::internal_window*>(toplevel);
    if (!client) {
        return nullptr;
    }
    return client->internalWindow();
}

template<typename T>
EffectWindowList getMainWindows(T* c)
{
    const auto leads = c->transient()->leads();
    EffectWindowList ret;
    ret.reserve(leads.size());
    std::transform(std::cbegin(leads), std::cend(leads), std::back_inserter(ret), [](auto client) {
        return client->render->effect.get();
    });
    return ret;
}

EffectWindowList effects_window_impl::mainWindows() const
{
    if (toplevel->control || toplevel->remnant) {
        return getMainWindows(toplevel);
    }
    return {};
}

WindowQuadList effects_window_impl::buildQuads(bool force) const
{
    return sceneWindow()->buildQuads(force);
}

void effects_window_impl::setData(int role, const QVariant& data)
{
    if (!data.isNull())
        dataMap[role] = data;
    else
        dataMap.remove(role);
    Q_EMIT effects->windowDataChanged(this, role);
}

QVariant effects_window_impl::data(int role) const
{
    return dataMap.value(role);
}

EffectWindow* effectWindow(Toplevel* w)
{
    return w->render->effect.get();
}

void effects_window_impl::elevate(bool elevate)
{
    effects->setElevatedWindow(this, elevate);
}

void effects_window_impl::registerThumbnail(basic_thumbnail_item* item)
{
    if (auto thumb = qobject_cast<window_thumbnail_item*>(item)) {
        insertThumbnail(thumb);
        connect(thumb, &QObject::destroyed, this, &effects_window_impl::thumbnailDestroyed);
        connect(thumb,
                &window_thumbnail_item::wIdChanged,
                this,
                &effects_window_impl::thumbnailTargetChanged);
    } else if (auto desktopThumb = qobject_cast<desktop_thumbnail_item*>(item)) {
        m_desktopThumbnails.append(desktopThumb);
        connect(desktopThumb,
                &QObject::destroyed,
                this,
                &effects_window_impl::desktopThumbnailDestroyed);
    }
}

void effects_window_impl::thumbnailDestroyed(QObject* object)
{
    // we know it is a window_thumbnail_item
    m_thumbnails.remove(static_cast<window_thumbnail_item*>(object));
}

void effects_window_impl::thumbnailTargetChanged()
{
    if (auto item = qobject_cast<window_thumbnail_item*>(sender())) {
        insertThumbnail(item);
    }
}

void effects_window_impl::insertThumbnail(window_thumbnail_item* item)
{
    EffectWindow* w = effects->findWindow(item->wId());
    if (w) {
        m_thumbnails.insert(item,
                            QPointer<effects_window_impl>(static_cast<effects_window_impl*>(w)));
    } else {
        m_thumbnails.insert(item, QPointer<effects_window_impl>());
    }
}

void effects_window_impl::desktopThumbnailDestroyed(QObject* object)
{
    // we know it is a desktop_thumbnail_item
    m_desktopThumbnails.removeAll(static_cast<desktop_thumbnail_item*>(object));
}

void effects_window_impl::minimize()
{
    if (toplevel->control) {
        win::set_minimized(toplevel, true);
    }
}

void effects_window_impl::unminimize()
{
    if (toplevel->control) {
        win::set_minimized(toplevel, false);
    }
}

void effects_window_impl::closeWindow()
{
    if (toplevel->control) {
        toplevel->closeWindow();
    }
}

void effects_window_impl::referencePreviousWindowPixmap()
{
    if (sw) {
        sw->reference_previous_buffer();
    }
}

void effects_window_impl::unreferencePreviousWindowPixmap()
{
    if (sw) {
        sw->unreference_previous_buffer();
    }
}

bool effects_window_impl::isManaged() const
{
    return managed;
}

bool effects_window_impl::isWaylandClient() const
{
    return waylandClient;
}

bool effects_window_impl::isX11Client() const
{
    return x11Client;
}

//****************************************
// effect_window_group_impl
//****************************************

EffectWindowList effect_window_group_impl::members() const
{
    const auto memberList = group->members;
    EffectWindowList ret;
    ret.reserve(memberList.size());
    std::transform(std::cbegin(memberList),
                   std::cend(memberList),
                   std::back_inserter(ret),
                   [](auto toplevel) { return toplevel->render->effect.get(); });
    return ret;
}

//****************************************
// effect_frame_impl
//****************************************

effect_frame_quick_scene::effect_frame_quick_scene(EffectFrameStyle style,
                                                   bool staticSize,
                                                   QPoint position,
                                                   Qt::Alignment alignment,
                                                   QObject* parent)
    : EffectQuickScene(parent)
    , m_style(style)
    , m_static(staticSize)
    , m_point(position)
    , m_alignment(alignment)
{

    QString name;
    switch (style) {
    case EffectFrameNone:
        name = QStringLiteral("none");
        break;
    case EffectFrameUnstyled:
        name = QStringLiteral("unstyled");
        break;
    case EffectFrameStyled:
        name = QStringLiteral("styled");
        break;
    }

    const QString defaultPath = QStringLiteral(KWIN_NAME "/frames/plasma/frame_%1.qml").arg(name);
    // TODO read from kwinApp()->config() "QmlPath" like Outline/OnScreenNotification
    // *if* someone really needs this to be configurable.
    const QString path = QStandardPaths::locate(QStandardPaths::GenericDataLocation, defaultPath);

    setSource(QUrl::fromLocalFile(path),
              QVariantMap{{QStringLiteral("effectFrame"), QVariant::fromValue(this)}});

    if (rootItem()) {
        connect(rootItem(),
                &QQuickItem::implicitWidthChanged,
                this,
                &effect_frame_quick_scene::reposition);
        connect(rootItem(),
                &QQuickItem::implicitHeightChanged,
                this,
                &effect_frame_quick_scene::reposition);
    }
}

effect_frame_quick_scene::~effect_frame_quick_scene() = default;

EffectFrameStyle effect_frame_quick_scene::style() const
{
    return m_style;
}

bool effect_frame_quick_scene::isStatic() const
{
    return m_static;
}

const QFont& effect_frame_quick_scene::font() const
{
    return m_font;
}

void effect_frame_quick_scene::setFont(const QFont& font)
{
    if (m_font == font) {
        return;
    }

    m_font = font;
    Q_EMIT fontChanged(font);
    reposition();
}

const QIcon& effect_frame_quick_scene::icon() const
{
    return m_icon;
}

void effect_frame_quick_scene::setIcon(const QIcon& icon)
{
    m_icon = icon;
    Q_EMIT iconChanged(icon);
    reposition();
}

const QSize& effect_frame_quick_scene::iconSize() const
{
    return m_iconSize;
}

void effect_frame_quick_scene::setIconSize(const QSize& iconSize)
{
    if (m_iconSize == iconSize) {
        return;
    }

    m_iconSize = iconSize;
    Q_EMIT iconSizeChanged(iconSize);
    reposition();
}

const QString& effect_frame_quick_scene::text() const
{
    return m_text;
}

void effect_frame_quick_scene::setText(const QString& text)
{
    if (m_text == text) {
        return;
    }

    m_text = text;
    Q_EMIT textChanged(text);
    reposition();
}

qreal effect_frame_quick_scene::frameOpacity() const
{
    return m_frameOpacity;
}

void effect_frame_quick_scene::setFrameOpacity(qreal frameOpacity)
{
    if (m_frameOpacity != frameOpacity) {
        m_frameOpacity = frameOpacity;
        Q_EMIT frameOpacityChanged(frameOpacity);
    }
}

bool effect_frame_quick_scene::crossFadeEnabled() const
{
    return m_crossFadeEnabled;
}

void effect_frame_quick_scene::setCrossFadeEnabled(bool enabled)
{
    if (m_crossFadeEnabled != enabled) {
        m_crossFadeEnabled = enabled;
        Q_EMIT crossFadeEnabledChanged(enabled);
    }
}

qreal effect_frame_quick_scene::crossFadeProgress() const
{
    return m_crossFadeProgress;
}

void effect_frame_quick_scene::setCrossFadeProgress(qreal progress)
{
    if (m_crossFadeProgress != progress) {
        m_crossFadeProgress = progress;
        Q_EMIT crossFadeProgressChanged(progress);
    }
}

Qt::Alignment effect_frame_quick_scene::alignment() const
{
    return m_alignment;
}

void effect_frame_quick_scene::setAlignment(Qt::Alignment alignment)
{
    if (m_alignment == alignment) {
        return;
    }

    m_alignment = alignment;
    reposition();
}

QPoint effect_frame_quick_scene::position() const
{
    return m_point;
}

void effect_frame_quick_scene::setPosition(const QPoint& point)
{
    if (m_point == point) {
        return;
    }

    m_point = point;
    reposition();
}

void effect_frame_quick_scene::reposition()
{
    if (!rootItem() || m_point.x() < 0 || m_point.y() < 0) {
        return;
    }

    QSizeF size;
    if (m_static) {
        size = rootItem()->size();
    } else {
        size = QSizeF(rootItem()->implicitWidth(), rootItem()->implicitHeight());
    }

    QRect geometry(QPoint(), size.toSize());

    if (m_alignment & Qt::AlignLeft)
        geometry.moveLeft(m_point.x());
    else if (m_alignment & Qt::AlignRight)
        geometry.moveLeft(m_point.x() - geometry.width());
    else
        geometry.moveLeft(m_point.x() - geometry.width() / 2);
    if (m_alignment & Qt::AlignTop)
        geometry.moveTop(m_point.y());
    else if (m_alignment & Qt::AlignBottom)
        geometry.moveTop(m_point.y() - geometry.height());
    else
        geometry.moveTop(m_point.y() - geometry.height() / 2);

    if (geometry == this->geometry()) {
        return;
    }

    setGeometry(geometry);
}

effect_frame_impl::effect_frame_impl(render::scene& scene,
                                     EffectFrameStyle style,
                                     bool staticSize,
                                     QPoint position,
                                     Qt::Alignment alignment)
    : QObject(nullptr)
    , EffectFrame()
    , scene{scene}
    , m_view{new effect_frame_quick_scene(style, staticSize, position, alignment, nullptr)}
{
    connect(
        m_view, &EffectQuickView::repaintNeeded, this, [this] { effects->addRepaint(geometry()); });
    connect(m_view,
            &EffectQuickView::geometryChanged,
            this,
            [this](const QRect& oldGeometry, const QRect& newGeometry) {
                this->scene.compositor.effects->addRepaint(oldGeometry);
                m_geometry = newGeometry;
                this->scene.compositor.effects->addRepaint(newGeometry);
            });
}

effect_frame_impl::~effect_frame_impl()
{
    // Effects often destroy their cached TextFrames in pre/postPaintScreen.
    // Destroying an OffscreenQuickView changes GL context, which we
    // must not do during effect rendering.
    // Delay destruction of the view until after the rendering.
    m_view->deleteLater();
}

Qt::Alignment effect_frame_impl::alignment() const
{
    return m_view->alignment();
}

void effect_frame_impl::setAlignment(Qt::Alignment alignment)
{
    m_view->setAlignment(alignment);
}

const QFont& effect_frame_impl::font() const
{
    return m_view->font();
}

void effect_frame_impl::setFont(const QFont& font)
{
    m_view->setFont(font);
}

void effect_frame_impl::free()
{
    m_view->hide();
}

const QRect& effect_frame_impl::geometry() const
{
    // Can't forward to EffectQuickView::geometry() because we return a reference.
    return m_geometry;
}

void effect_frame_impl::setGeometry(const QRect& geometry, bool force)
{
    Q_UNUSED(force)
    m_view->setGeometry(geometry);
}

const QIcon& effect_frame_impl::icon() const
{
    return m_view->icon();
}

void effect_frame_impl::setIcon(const QIcon& icon)
{
    m_view->setIcon(icon);

    if (m_view->iconSize().isEmpty()
        && !icon.availableSizes().isEmpty()) { // Set a size if we don't already have one
        setIconSize(icon.availableSizes().constFirst());
    }
}

const QSize& effect_frame_impl::iconSize() const
{
    return m_view->iconSize();
}

void effect_frame_impl::setIconSize(const QSize& size)
{
    m_view->setIconSize(size);
}

void effect_frame_impl::setPosition(const QPoint& point)
{
    m_view->setPosition(point);
}

void effect_frame_impl::render(const QRegion& region, double opacity, double frameOpacity)
{
    Q_UNUSED(region);

    if (!m_view->rootItem()) {
        return;
    }

    m_view->show();

    m_view->setOpacity(opacity);
    m_view->setFrameOpacity(frameOpacity);

    scene.compositor.effects->renderEffectQuickView(m_view);
}

const QString& effect_frame_impl::text() const
{
    return m_view->text();
}

void effect_frame_impl::setText(const QString& text)
{
    m_view->setText(text);
}

EffectFrameStyle effect_frame_impl::style() const
{
    return m_view->style();
}

bool effect_frame_impl::isCrossFade() const
{
    return m_view->crossFadeEnabled();
}

void effect_frame_impl::enableCrossFade(bool enable)
{
    m_view->setCrossFadeEnabled(enable);
}

qreal effect_frame_impl::crossFadeProgress() const
{
    return m_view->crossFadeProgress();
}

void effect_frame_impl::setCrossFadeProgress(qreal progress)
{
    m_view->setCrossFadeProgress(progress);
}

} // namespace
