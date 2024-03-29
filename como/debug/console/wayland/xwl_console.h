/*
    SPDX-FileCopyrightText: 2023 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "input_device_model.h"
#include "input_filter.h"
#include "model_helpers.h"
#include "xwl_surface_tree_model.h"
#include <como/debug/console/wayland/wayland_console.h>

#include <como/debug/console/console.h>
#include <como/input/redirect_qobject.h>
#include <como_export.h>

namespace como::debug
{

template<typename Space>
class COMO_EXPORT xwl_console : public console<Space>
{
public:
    xwl_console(Space& space)
        : console<Space>(space)
    {
        this->m_ui->windowsView->setItemDelegate(new wayland_console_delegate(this));

        auto windows_model = new wayland_console_model(this);
        model_setup_connections(*windows_model, space);
        wayland_model_setup_connections(*windows_model, space);
        this->m_ui->windowsView->setModel(windows_model);

        this->m_ui->surfacesView->setModel(new xwl_surface_tree_model(space, this));

        if constexpr (requires(decltype(space) space) { space.base.mod.input->mod.dbus; }) {
            auto device_model = new input_device_model(this);
            setup_input_device_model(*device_model, *space.base.mod.input->mod.dbus);
            this->m_ui->inputDevicesView->setModel(device_model);
            this->m_ui->inputDevicesView->setItemDelegate(new wayland_console_delegate(this));
        }

        QObject::connect(
            this->m_ui->tabWidget, &QTabWidget::currentChanged, this, [this, &space](int index) {
                // delay creation of input event filter until the tab is selected
                if (!m_inputFilter && index == 2) {
                    m_inputFilter = std::make_unique<input_filter<typename Space::input_t>>(
                        *space.input, this->m_ui->inputTextEdit);
                    space.input->m_spies.push_back(m_inputFilter.get());
                }
                if (index == 5) {
                    update_keyboard_tab();
                    QObject::connect(space.input->qobject.get(),
                                     &input::redirect_qobject::keyStateChanged,
                                     this,
                                     &xwl_console::update_keyboard_tab);
                }
            });

        // TODO(romangg): Can we do that on Wayland differently?
        this->setWindowFlags(Qt::X11BypassWindowManagerHint);
    }

private:
    template<typename T>
    QString keymapComponentToString(xkb_keymap* map,
                                    const T& count,
                                    std::function<const char*(xkb_keymap*, T)> f)
    {
        QString text = QStringLiteral("<ul>");
        for (T i = 0; i < count; i++) {
            text.append(QStringLiteral("<li>%1</li>").arg(QString::fromLocal8Bit(f(map, i))));
        }
        text.append(QStringLiteral("</ul>"));
        return text;
    }

    template<typename T>
    QString stateActiveComponents(xkb_state* state,
                                  const T& count,
                                  std::function<int(xkb_state*, T)> f,
                                  std::function<const char*(xkb_keymap*, T)> name)
    {
        QString text = QStringLiteral("<ul>");
        xkb_keymap* map = xkb_state_get_keymap(state);
        for (T i = 0; i < count; i++) {
            if (f(state, i) == 1) {
                text.append(
                    QStringLiteral("<li>%1</li>").arg(QString::fromLocal8Bit(name(map, i))));
            }
        }
        text.append(QStringLiteral("</ul>"));
        return text;
    }

    void update_keyboard_tab()
    {
        auto xkb = input::xkb::get_primary_xkb_keyboard(*this->space.base.mod.input);
        auto keymap = xkb->keymap->raw;

        this->m_ui->layoutsLabel->setText(keymapComponentToString<xkb_layout_index_t>(
            keymap, xkb_keymap_num_layouts(keymap), &xkb_keymap_layout_get_name));
        this->m_ui->currentLayoutLabel->setText(xkb_keymap_layout_get_name(keymap, xkb->layout));
        this->m_ui->modifiersLabel->setText(keymapComponentToString<xkb_mod_index_t>(
            keymap, xkb_keymap_num_mods(keymap), &xkb_keymap_mod_get_name));
        this->m_ui->ledsLabel->setText(keymapComponentToString<xkb_led_index_t>(
            keymap, xkb_keymap_num_leds(keymap), &xkb_keymap_led_get_name));
        this->m_ui->activeLedsLabel->setText(
            stateActiveComponents<xkb_led_index_t>(xkb->state,
                                                   xkb_keymap_num_leds(keymap),
                                                   &xkb_state_led_index_is_active,
                                                   &xkb_keymap_led_get_name));

        using namespace std::placeholders;
        auto modActive = std::bind(xkb_state_mod_index_is_active, _1, _2, XKB_STATE_MODS_EFFECTIVE);
        this->m_ui->activeModifiersLabel->setText(stateActiveComponents<xkb_mod_index_t>(
            xkb->state, xkb_keymap_num_mods(keymap), modActive, &xkb_keymap_mod_get_name));
    }

    std::unique_ptr<input_filter<typename Space::input_t>> m_inputFilter;
};

}
