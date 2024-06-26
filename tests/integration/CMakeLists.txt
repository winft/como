# SPDX-FileCopyrightText: 2023 Roman Gilg <subdiff@gmail.com>
#
# SPDX-License-Identifier: GPL-2.0-or-later

add_subdirectory(helper)
add_subdirectory(fakes)

add_executable(tests
  lib/client.cpp
  lib/helpers.cpp
  lib/main.cpp
  lib/setup.cpp
  # integration tests
  activation.cpp
  bindings.cpp
  buffer_size_change.cpp
  client_machine.cpp
  night_color.cpp
  dbus_interface.cpp
  debug_console.cpp
  decoration_input.cpp
  desktop_window_x11.cpp
  no_crash_aurorae_destroy_deco.cpp
  no_crash_cancel_animation.cpp
  no_crash_cursor_physical_size_empty.cpp
  no_crash_empty_deco.cpp
  no_crash_glxgears.cpp
  no_crash_no_border.cpp
  no_crash_reinitialize_compositor.cpp
  no_crash_useractions_menu.cpp
  gestures.cpp
  global_shortcuts.cpp
  idle_inhibition.cpp
  idle.cpp
  input_method.cpp
  input_stacking_order.cpp
  internal_window.cpp
  keyboard_keymap.cpp
  keyboard_layout.cpp
  keymap_creation_failure.cpp
  layer_shell.cpp
  lockscreen.cpp
  maximize.cpp
  modifier_only_shortcut.cpp
  move_resize_window.cpp
  no_global_shortcuts.cpp
  no_xdg_runtime_dir.cpp
  placement.cpp
  plasma_surface.cpp
  plasma_window.cpp
  platform_cursor.cpp
  pointer_constraints.cpp
  quick_tiling.cpp
  opengl_shadow.cpp
  scene_opengl.cpp
  qpainter_shadow.cpp
  scene_qpainter.cpp
  screen_changes.cpp
  screen_edges.cpp
  screen_edge_window_show.cpp
  screens.cpp
  showing_desktop.cpp
  stacking_order.cpp
  struts.cpp
  subspace.cpp
  tabbox.cpp
  touch_input.cpp
  transient_placement.cpp
  virtual_keyboard.cpp
  window_rules.cpp
  window_selection.cpp
  x11_client.cpp
  xcb_size_hints.cpp
  xcb_wrapper.cpp
  xdg_activation.cpp
  xdg-shell_rules.cpp
  xdg-shell_window.cpp
  xwayland_input.cpp
  xwayland_selections.cpp
  # effect tests
  effects/fade.cpp
  effects/maximize_animation.cpp
  effects/minimize_animation.cpp
  effects/popup_open_close_animation.cpp
  effects/scripted_effects.cpp
  effects/slidingpopups.cpp
  effects/subspace_switching_animation.cpp
  effects/window_open_close_animation.cpp
  effects/translucency.cpp
  # scripting tests
  scripting/minimize_all.cpp
  scripting/screen_edge.cpp
  # unit tests
  ../unit/effects/opengl_platform.cpp
  ../unit/effects/timeline.cpp
  ../unit/effects/window_quad_list.cpp
  ../unit/on_screen_notifications.cpp
  ../unit/opengl_context_attribute_builder.cpp
  ../unit/tabbox/tabbox_client_model.cpp
  ../unit/tabbox/tabbox_config.cpp
  ../unit/tabbox/tabbox_handler.cpp
  ../unit/gestures.cpp
  ../unit/xcb_window.cpp
  ../unit/xkb.cpp
  # unit tests support
  ../unit/effects/mock_gl.cpp
  ../unit/tabbox/mock_tabbox_client.cpp
  ../unit/tabbox/mock_tabbox_handler.cpp
  ../../como/render/backend/x11/glx_context_attribute_builder.cpp
)

target_compile_definitions(tests PRIVATE USE_XWL=1)
set_target_properties(tests PROPERTIES UNITY_BUILD ON)

target_link_libraries(tests
PRIVATE
  desktop-kde-wl
  como::wayland
  como::xwayland
  script
  Qt::Test
  Catch2::Catch2WithMain
  KF6::Crash
  KF6::WindowSystem
  WraplandClient
)

add_executable(tests-wl
  lib/client.cpp
  lib/helpers.cpp
  lib/main.cpp
  lib/setup.cpp
  # integration tests
  activation.cpp
  bindings.cpp
  buffer_size_change.cpp
  decoration_input.cpp
  gestures.cpp
  idle.cpp
  idle_inhibition.cpp
  input_method.cpp
  input_stacking_order.cpp
  internal_window.cpp
  keyboard_keymap.cpp
  keyboard_layout.cpp
  keymap_creation_failure.cpp
  layer_shell.cpp
  maximize.cpp
  modifier_only_shortcut.cpp
  night_color.cpp
  no_crash_cancel_animation.cpp
  no_crash_cursor_physical_size_empty.cpp
  no_crash_no_border.cpp
  no_crash_reinitialize_compositor.cpp
  no_crash_useractions_menu.cpp
  no_global_shortcuts.cpp
  no_xdg_runtime_dir.cpp
  opengl_shadow.cpp
  placement.cpp
  plasma_surface.cpp
  platform_cursor.cpp
  pointer_constraints.cpp
  pointer_input.cpp
  qpainter_shadow.cpp
  scene_opengl.cpp
  screen_changes.cpp
  screens.cpp
  showing_desktop.cpp
  subspace.cpp
  tabbox.cpp
  touch_input.cpp
  transient_placement.cpp
  virtual_keyboard.cpp
  window_selection.cpp
  xdg-shell_rules.cpp
  xdg-shell_window.cpp
  xdg_activation.cpp
  # effect tests
  effects/fade.cpp
  effects/maximize_animation.cpp
  effects/minimize_animation.cpp
  effects/popup_open_close_animation.cpp
  effects/scripted_effects.cpp
  effects/subspace_switching_animation.cpp
  effects/window_open_close_animation.cpp
  # scripting tests
  scripting/minimize_all.cpp
  scripting/screen_edge.cpp
)

target_compile_definitions(tests-wl PRIVATE USE_XWL=0)
set_target_properties(tests-wl PROPERTIES UNITY_BUILD ON)

target_link_libraries(tests-wl
PRIVATE
  desktop-kde-wl
  como::wayland
  script
  Qt::Test
  Catch2::Catch2WithMain
  KF6::Crash
  KF6::WindowSystem
  WraplandClient
)

include(Catch)
catch_discover_tests(tests)
catch_discover_tests(tests-wl TEST_SUFFIX " (wl)")
