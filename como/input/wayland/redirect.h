/*
    SPDX-FileCopyrightText: 2021 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "cursor.h"
#include "fake/devices.h"
#include "input_method.h"
#include "keyboard_redirect.h"
#include "pointer_redirect.h"
#include "tablet_redirect.h"
#include "touch_redirect.h"

// TODO(romangg): should only be included when COMO_BUILD_TABBOX is defined.
#include <como/input/filters/tabbox.h>

#include <como/base/wayland/output_helpers.h>
#include <como/input/dbus/tablet_mode_manager.h>
#include <como/input/filters/decoration_event.h>
#include <como/input/filters/dpms.h>
#include <como/input/filters/drag_and_drop.h>
#include <como/input/filters/effects.h>
#include <como/input/filters/fake_tablet.h>
#include <como/input/filters/forward.h>
#include <como/input/filters/global_shortcut.h>
#include <como/input/filters/internal_window.h>
#include <como/input/filters/lock_screen.h>
#include <como/input/filters/move_resize.h>
#include <como/input/filters/popup.h>
#include <como/input/filters/screen_edge.h>
#include <como/input/filters/virtual_terminal.h>
#include <como/input/filters/window_action.h>
#include <como/input/filters/window_selector.h>
#include <como/input/redirect_qobject.h>
#include <como/input/spies/activity.h>
#include <como/input/spies/touch_hide_cursor.h>

#include <KConfigWatcher>
#include <Wrapland/Server/display.h>
#include <Wrapland/Server/fake_input.h>
#include <Wrapland/Server/keyboard_pool.h>
#include <Wrapland/Server/pointer_pool.h>
#include <Wrapland/Server/seat.h>
#include <Wrapland/Server/touch_pool.h>
#include <Wrapland/Server/virtual_keyboard_v1.h>
#include <unordered_map>

namespace como::input::wayland
{

template<typename Space>
class redirect
{
public:
    using type = redirect<Space>;
    using platform_t = typename Space::base_t::input_t;
    using space_t = Space;
    using window_t = typename space_t::window_t;
    using dpms_filter_t = input::dpms_filter<type>;
    using event_spy_t = input::event_spy<type>;

    redirect(Space& space)
        : qobject{std::make_unique<redirect_qobject>()}
        , platform{*space.base.mod.input}
        , space{space}
        , config_watcher{KConfigWatcher::create(platform.config.main)}
        , input_method{std::make_unique<wayland::input_method<type>>(*this)}
        , tablet_mode_manager{std::make_unique<dbus::tablet_mode_manager<type>>(*this)}
    {
        setup_workspace();

        using base_t = std::decay_t<decltype(platform.base)>;
        QObject::connect(platform.base.qobject.get(),
                         &base_t::qobject_t::output_added,
                         this->qobject.get(),
                         [this] { base::wayland::check_outputs_on(this->platform.base); });
        QObject::connect(platform.base.qobject.get(),
                         &base_t::qobject_t::output_removed,
                         this->qobject.get(),
                         [this] { base::wayland::check_outputs_on(this->platform.base); });
    }

    ~redirect()
    {
        auto const filters = m_filters;
        for (auto filter : filters) {
            delete filter;
        }

        auto const spies = m_spies;
        for (auto spy : spies) {
            delete spy;
        }
    }

    /**
     * @return const QPointF& The current global pointer position
     */
    QPointF globalPointer() const
    {
        return pointer->pos();
    }

    Qt::MouseButtons qtButtonStates() const
    {
        return pointer->buttons();
    }

    void turn_outputs_on()
    {
        base::wayland::turn_outputs_on(platform.base, dpms_filter);
    }

    void warp_pointer(QPointF const& pos, uint32_t time)
    {
        if (platform.pointers.empty()) {
            return;
        }
        pointer->processMotion(pos, time, platform.pointers.front());
    }

    void cancelTouch()
    {
        touch->cancel();
    }

    bool has_tablet_mode_switch()
    {
        return std::any_of(platform.switches.cbegin(), platform.switches.cend(), [](auto dev) {
            return dev->control->is_tablet_mode_switch();
        });
    }

    /**
     * Starts an interactive window selection process.
     *
     * Once the user selected a window the @p callback is invoked with the selected Toplevel as
     * argument. In case the user cancels the interactive window selection or selecting a window is
     * currently not possible (e.g. screen locked) the @p callback is invoked with a @c nullptr
     * argument.
     *
     * During the interactive window selection the cursor is turned into a crosshair cursor unless
     * @p cursorName is provided. The argument @p cursorName is a QByteArray instead of
     * Qt::CursorShape to support the "pirate" cursor for kill window which is not wrapped by
     * Qt::CursorShape.
     *
     * @param callback The function to invoke once the interactive window selection ends
     * @param cursorName The optional name of the cursor shape to use, default is crosshair
     */
    void start_interactive_window_selection(std::function<void(std::optional<window_t>)> callback,
                                            QByteArray const& cursorName = {})
    {
        if (window_selector->isActive()) {
            callback({});
            return;
        }
        window_selector->start(callback);
        pointer->setWindowSelectionCursor(cursorName.toStdString());
    }

    /**
     * Starts an interactive position selection process.
     *
     * Once the user selected a position on the screen the @p callback is invoked with
     * the selected point as argument. In case the user cancels the interactive position selection
     * or selecting a position is currently not possible (e.g. screen locked) the @p callback
     * is invoked with a point at @c -1 as x and y argument.
     *
     * During the interactive window selection the cursor is turned into a crosshair cursor.
     *
     * @param callback The function to invoke once the interactive position selection ends
     */
    void start_interactive_position_selection(std::function<void(QPoint const&)> callback)
    {
        if (window_selector->isActive()) {
            callback(QPoint(-1, -1));
            return;
        }
        window_selector->start(callback);
        pointer->setWindowSelectionCursor({});
    }

    bool isSelectingWindow() const
    {
        // TODO(romangg): This function is called before setup_filters runs (from setup_workspace).
        //                Can we ensure it's only called afterwards and remove the nullptr check?
        return window_selector && window_selector->isActive();
    }

    /**
     * Adds the @p filter to the list of event filters at the last relevant position.
     *
     * Install the filter at the back of the list for a X compositor, immediately before
     * the forward filter for a Wayland compositor.
     */
    void append_filter(event_filter<type>* filter)
    {
        Q_ASSERT(!contains(m_filters, filter));
        m_filters.insert(m_filters_install_iterator, filter);
    }

    /**
     * Adds the @p filter to the list of event filters and makes it the first
     * event filter in processing.
     *
     * Note: the event filter will get events before the lock screen can get them, thus
     * this is a security relevant method.
     */
    void prependInputEventFilter(event_filter<type>* filter)
    {
        Q_ASSERT(!contains(m_filters, filter));
        m_filters.insert(m_filters.begin(), filter);
    }

    void uninstallInputEventFilter(event_filter<type>* filter)
    {
        remove_all(m_filters, filter);
    }

    std::unique_ptr<keyboard_redirect<type>> keyboard;
    std::unique_ptr<pointer_redirect<type>> pointer;
    std::unique_ptr<tablet_redirect<type>> tablet;
    std::unique_ptr<touch_redirect<type>> touch;

    std::unique_ptr<wayland::cursor<type>> cursor;

    std::list<event_filter<type>*> m_filters;
    std::vector<event_spy_t*> m_spies;

    std::unique_ptr<input::dpms_filter<type>> dpms_filter;

    std::unique_ptr<redirect_qobject> qobject;
    platform_t& platform;
    Space& space;

private:
    template<typename Dev>
    static void unset_focus(Dev&& dev)
    {
        dev->focusUpdate(dev->focus.window, {});
    }

    void setup_workspace()
    {
        reconfigure();
        QObject::connect(config_watcher.data(),
                         &KConfigWatcher::configChanged,
                         qobject.get(),
                         [this](auto const& group) {
                             if (group.name() == QLatin1String("Keyboard")) {
                                 reconfigure();
                             }
                         });

        cursor = std::make_unique<wayland::cursor<type>>(*this);

        pointer = std::make_unique<wayland::pointer_redirect<type>>(this);
        keyboard = std::make_unique<wayland::keyboard_redirect<type>>(this);
        touch = std::make_unique<wayland::touch_redirect<type>>(this);
        tablet = std::make_unique<wayland::tablet_redirect<type>>(this);

        setup_devices();

        fake_input
            = std::make_unique<Wrapland::Server::FakeInput>(platform.base.server->display.get());
        QObject::connect(fake_input.get(),
                         &Wrapland::Server::FakeInput::deviceCreated,
                         qobject.get(),
                         [this](auto device) { handle_fake_input_device_added(device); });
        QObject::connect(fake_input.get(),
                         &Wrapland::Server::FakeInput::device_destroyed,
                         qobject.get(),
                         [this](auto device) { fake_devices.erase(device); });

        QObject::connect(platform.virtual_keyboard.get(),
                         &Wrapland::Server::virtual_keyboard_manager_v1::keyboard_created,
                         qobject.get(),
                         [this](auto device) { handle_virtual_keyboard_added(device); });

        keyboard->init();
        pointer->init();
        touch->init();
        tablet->init();

        setup_filters();
    }

    void setup_devices()
    {
        for (auto pointer : platform.pointers) {
            handle_pointer_added(pointer);
        }
        QObject::connect(platform.qobject.get(),
                         &platform_qobject::pointer_added,
                         qobject.get(),
                         [this](auto pointer) { handle_pointer_added(pointer); });
        QObject::connect(
            platform.qobject.get(), &platform_qobject::pointer_removed, qobject.get(), [this]() {
                if (platform.pointers.empty()) {
                    auto seat = platform.base.server->seat();
                    unset_focus(pointer.get());
                    seat->setHasPointer(false);
                }
            });

        for (auto keyboard : platform.keyboards) {
            handle_keyboard_added(keyboard);
        }
        QObject::connect(platform.qobject.get(),
                         &platform_qobject::keyboard_added,
                         qobject.get(),
                         [this](auto keys) { handle_keyboard_added(keys); });
        QObject::connect(
            platform.qobject.get(), &platform_qobject::keyboard_removed, qobject.get(), [this]() {
                if (platform.keyboards.empty()) {
                    auto seat = platform.base.server->seat();
                    seat->setFocusedKeyboardSurface(nullptr);
                    seat->setHasKeyboard(false);
                }
            });

        for (auto touch : platform.touchs) {
            handle_touch_added(touch);
        }
        QObject::connect(platform.qobject.get(),
                         &platform_qobject::touch_added,
                         qobject.get(),
                         [this](auto touch) { handle_touch_added(touch); });
        QObject::connect(
            platform.qobject.get(), &platform_qobject::touch_removed, qobject.get(), [this]() {
                if (platform.touchs.empty()) {
                    auto seat = platform.base.server->seat();
                    unset_focus(touch.get());
                    seat->setHasTouch(false);
                }
            });

        for (auto switch_dev : platform.switches) {
            handle_switch_added(switch_dev);
        }
        QObject::connect(platform.qobject.get(),
                         &platform_qobject::switch_added,
                         qobject.get(),
                         [this](auto dev) { handle_switch_added(dev); });
    }

    void setup_filters()
    {
        auto const has_global_shortcuts = platform.base.server->has_global_shortcut_support();

        if (platform.base.session->hasSessionControl() && has_global_shortcuts) {
            m_filters.emplace_back(new virtual_terminal_filter<type>(*this));
        }

        m_spies.push_back(new activity_spy(*this));
        m_spies.push_back(new touch_hide_cursor_spy(*this));
        m_filters.emplace_back(new drag_and_drop_filter<type>(*this));
        m_filters.emplace_back(new lock_screen_filter<type>(*this));
        m_filters.emplace_back(new popup_filter<type>(*this));

        window_selector = new window_selector_filter<type>(*this);
        m_filters.push_back(window_selector);

        if (has_global_shortcuts) {
            m_filters.emplace_back(new screen_edge_filter(*this));
        }

#if COMO_BUILD_TABBOX
        m_filters.emplace_back(new tabbox_filter(*this));
#endif

        if (has_global_shortcuts) {
            m_filters.emplace_back(new global_shortcut_filter(*this));
        }
        m_filters.emplace_back(new effects_filter(*this));
        m_filters.emplace_back(new move_resize_filter(*this));
        m_filters.emplace_back(new decoration_event_filter(*this));
        m_filters.emplace_back(new internal_window_filter(*this));

        m_filters.emplace_back(new window_action_filter(*this));
        m_filters_install_iterator
            = m_filters.insert(m_filters.cend(), new forward_filter<type>(*this));
        m_filters.emplace_back(new fake_tablet_filter(*this));
    }

    void reconfigure()
    {
        auto input_config = config_watcher->config();
        auto const group = input_config->group(QStringLiteral("Keyboard"));

        auto delay = group.readEntry("RepeatDelay", 660);
        auto rate = group.readEntry("RepeatRate", 25);
        auto const repeat = group.readEntry("KeyRepeat", "repeat");

        // When the clients will repeat the character or turn repeat key events into an accent
        // character selection, we want to tell the clients that we are indeed repeating keys.
        auto enabled = repeat == QLatin1String("accent") || repeat == QLatin1String("repeat");

        if (auto seat = platform.base.server->seat(); seat->hasKeyboard()) {
            seat->keyboards().set_repeat_info(enabled ? rate : 0, delay);
        }
    }

    void handle_pointer_added(input::pointer* pointer)
    {
        auto pointer_red = this->pointer.get();

        QObject::connect(pointer,
                         &pointer::button_changed,
                         pointer_red->qobject.get(),
                         [pointer_red](auto const& event) { pointer_red->process_button(event); });

        QObject::connect(pointer,
                         &pointer::motion,
                         pointer_red->qobject.get(),
                         [pointer_red](auto const& event) { pointer_red->process_motion(event); });
        QObject::connect(
            pointer,
            &pointer::motion_absolute,
            pointer_red->qobject.get(),
            [pointer_red](auto const& event) { pointer_red->process_motion_absolute(event); });

        QObject::connect(pointer,
                         &pointer::axis_changed,
                         pointer_red->qobject.get(),
                         [pointer_red](auto const& event) { pointer_red->process_axis(event); });

        QObject::connect(
            pointer,
            &pointer::pinch_begin,
            pointer_red->qobject.get(),
            [pointer_red](auto const& event) { pointer_red->process_pinch_begin(event); });
        QObject::connect(
            pointer,
            &pointer::pinch_update,
            pointer_red->qobject.get(),
            [pointer_red](auto const& event) { pointer_red->process_pinch_update(event); });
        QObject::connect(
            pointer,
            &pointer::pinch_end,
            pointer_red->qobject.get(),
            [pointer_red](auto const& event) { pointer_red->process_pinch_end(event); });

        QObject::connect(
            pointer,
            &pointer::swipe_begin,
            pointer_red->qobject.get(),
            [pointer_red](auto const& event) { pointer_red->process_swipe_begin(event); });
        QObject::connect(
            pointer,
            &pointer::swipe_update,
            pointer_red->qobject.get(),
            [pointer_red](auto const& event) { pointer_red->process_swipe_update(event); });
        QObject::connect(
            pointer,
            &pointer::swipe_end,
            pointer_red->qobject.get(),
            [pointer_red](auto const& event) { pointer_red->process_swipe_end(event); });

        QObject::connect(
            pointer,
            &pointer::hold_begin,
            pointer_red->qobject.get(),
            [pointer_red](auto const& event) { pointer_red->process_hold_begin(event); });
        QObject::connect(
            pointer,
            &pointer::hold_end,
            pointer_red->qobject.get(),
            [pointer_red](auto const& event) { pointer_red->process_hold_end(event); });

        QObject::connect(pointer, &pointer::frame, pointer_red->qobject.get(), [pointer_red] {
            pointer_red->process_frame();
        });

        auto seat = platform.base.server->seat();
        if (!seat->hasPointer()) {
            seat->setHasPointer(true);
            device_redirect_update_focus(this->pointer.get());
        }
    }

    void handle_keyboard_added(input::keyboard* keyboard)
    {
        auto keyboard_red = this->keyboard.get();
        auto seat = platform.base.server->seat();

        QObject::connect(keyboard,
                         &keyboard::key_changed,
                         keyboard_red->qobject.get(),
                         [keyboard_red](auto const& event) { keyboard_red->process_key(event); });
        QObject::connect(
            keyboard,
            &keyboard::modifiers_changed,
            keyboard_red->qobject.get(),
            [keyboard_red](auto const& event) { keyboard_red->process_modifiers(event); });

        if (!seat->hasKeyboard()) {
            seat->setHasKeyboard(true);
            this->keyboard->update();
            reconfigure();
        }

        keyboard->xkb->forward_modifiers_impl = [seat](auto keymap, auto&& mods, auto layout) {
            seat->keyboards().set_keymap(keymap->cache);
            seat->keyboards().update_modifiers(mods.depressed, mods.latched, mods.locked, layout);
        };

        xkb::keyboard_update_from_default(platform.xkb, *keyboard->xkb);

        platform.update_keyboard_leds(keyboard->xkb->leds);
        update_key_state(*platform.base.server->key_state, keyboard->xkb->leds);

        QObject::connect(keyboard->xkb->qobject.get(),
                         &xkb::keyboard_qobject::leds_changed,
                         platform.base.server->qobject.get(),
                         [&server = platform.base.server](auto&& leds) {
                             update_key_state(*server->key_state, leds);
                         });
        QObject::connect(keyboard->xkb->qobject.get(),
                         &xkb::keyboard_qobject::leds_changed,
                         platform.qobject.get(),
                         [this](auto leds) { platform.update_keyboard_leds(leds); });
    }

    void handle_touch_added(input::touch* touch)
    {
        auto touch_red = this->touch.get();

        QObject::connect(touch->qobject.get(),
                         &touch_qobject::down,
                         touch_red->qobject.get(),
                         [touch_red](auto const& event) { touch_red->process_down(event); });
        QObject::connect(touch->qobject.get(),
                         &touch_qobject::up,
                         touch_red->qobject.get(),
                         [touch_red](auto const& event) { touch_red->process_up(event); });
        QObject::connect(touch->qobject.get(),
                         &touch_qobject::motion,
                         touch_red->qobject.get(),
                         [touch_red](auto const& event) { touch_red->process_motion(event); });
        QObject::connect(touch->qobject.get(),
                         &touch_qobject::cancel,
                         touch_red->qobject.get(),
                         [touch_red] { touch_red->cancel(); });
        QObject::connect(touch->qobject.get(),
                         &touch_qobject::frame,
                         touch_red->qobject.get(),
                         [touch_red] { touch_red->frame(); });

        auto seat = platform.base.server->seat();
        if (!seat->hasTouch()) {
            seat->setHasTouch(true);
            device_redirect_update_focus(this->touch.get());
        }
    }

    void handle_switch_added(input::switch_device* switch_device)
    {
        QObject::connect(
            switch_device, &switch_device::toggle, qobject.get(), [this](auto const& event) {
                if (event.type == switch_type::tablet_mode) {
                    Q_EMIT qobject->has_tablet_mode_switch_changed(event.state == switch_state::on);
                }
            });
    }

    void handle_fake_input_device_added(Wrapland::Server::FakeInputDevice* device)
    {
        QObject::connect(device,
                         &Wrapland::Server::FakeInputDevice::authenticationRequested,
                         qobject.get(),
                         [device](auto const& /*application*/, auto const& /*reason*/) {
                             // TODO: make secure
                             device->setAuthentication(true);
                         });

        auto devices = fake::devices<type>(*this, device);
        fake_devices.insert({device, std::move(devices)});
    }

    void handle_virtual_keyboard_added(Wrapland::Server::virtual_keyboard_v1* virtual_keyboard)
    {
        auto keyboard
            = std::make_unique<input::keyboard>(platform.xkb.context, platform.xkb.compose_table);
        auto keyboard_ptr = keyboard.get();

        QObject::connect(virtual_keyboard,
                         &Wrapland::Server::virtual_keyboard_v1::resourceDestroyed,
                         keyboard_ptr,
                         [this, virtual_keyboard] {
                             auto it = virtual_keyboards.find(virtual_keyboard);
                             assert(it != virtual_keyboards.end());
                             platform_remove_keyboard(it->second.get(), platform);
                             virtual_keyboards.erase(it);
                         });

        QObject::connect(virtual_keyboard,
                         &Wrapland::Server::virtual_keyboard_v1::keymap,
                         keyboard_ptr,
                         [keyboard_ptr](auto /*format*/, auto fd, auto size) {
                             // TODO(romangg): Should we check the format?
                             keyboard_ptr->xkb->install_keymap(fd, size);
                         });

        QObject::connect(virtual_keyboard,
                         &Wrapland::Server::virtual_keyboard_v1::key,
                         keyboard_ptr,
                         [keyboard_ptr](auto time, auto key, auto state) {
                             Q_EMIT keyboard_ptr->key_changed(
                                 {key,
                                  state == Wrapland::Server::key_state::pressed
                                      ? key_state::pressed
                                      : key_state::released,
                                  false,
                                  {keyboard_ptr, time}});
                         });

        QObject::connect(virtual_keyboard,
                         &Wrapland::Server::virtual_keyboard_v1::modifiers,
                         keyboard_ptr,
                         [keyboard_ptr](auto depressed, auto latched, auto locked, auto group) {
                             Q_EMIT keyboard_ptr->modifiers_changed(
                                 {depressed, latched, locked, group, {keyboard_ptr}});
                         });

        virtual_keyboards.insert({virtual_keyboard, std::move(keyboard)});
        platform_add_keyboard(keyboard_ptr, platform);
    }

    KConfigWatcher::Ptr config_watcher;

    std::unique_ptr<wayland::input_method<type>> input_method;
    std::unique_ptr<dbus::tablet_mode_manager<type>> tablet_mode_manager;
    std::unique_ptr<Wrapland::Server::FakeInput> fake_input;

    std::unordered_map<Wrapland::Server::FakeInputDevice*, fake::devices<type>> fake_devices;
    std::unordered_map<Wrapland::Server::virtual_keyboard_v1*, std::unique_ptr<input::keyboard>>
        virtual_keyboards;

    typename std::list<event_filter<type>*>::const_iterator m_filters_install_iterator;

    window_selector_filter<type>* window_selector{nullptr};

private:
    static void update_key_state(Wrapland::Server::KeyState& key_state, input::keyboard_leds leds)
    {
        using key = Wrapland::Server::KeyState::Key;
        using state = Wrapland::Server::KeyState::State;

        key_state.setState(key::CapsLock,
                           flags(leds & input::keyboard_leds::caps_lock) ? state::Locked
                                                                         : state::Unlocked);
        key_state.setState(key::NumLock,
                           flags(leds & input::keyboard_leds::num_lock) ? state::Locked
                                                                        : state::Unlocked);
        key_state.setState(key::ScrollLock,
                           flags(leds & input::keyboard_leds::scroll_lock) ? state::Locked
                                                                           : state::Unlocked);
    }
};

}
