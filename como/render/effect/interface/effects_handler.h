/*
    SPDX-FileCopyrightText: 2022 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <como_export.h>
#include <render/effect/interface/effect_integration.h>
#include <render/effect/interface/paint_data.h>
#include <render/effect/interface/types.h>
#include <win/subspace.h>

#include <KSharedConfig>

class QAction;
class QKeyEvent;
class QQmlEngine;

struct xcb_connection_t;

namespace Wrapland::Server
{
class Surface;
class Display;
}

namespace como
{

class EffectFrame;
class OffscreenQuickView;
class EffectScreen;
class WindowQuadList;

/**
 * @short Manager class that handles all the effects.
 *
 * This class creates Effect objects and calls it's appropriate methods.
 *
 * Effect objects can call methods of this class to interact with the
 *  workspace, e.g. to activate or move a specific window, change current
 *  desktop or create a special input window to receive mouse and keyboard
 *  events.
 */
class COMO_EXPORT EffectsHandler : public QObject
{
    Q_OBJECT
    Q_PROPERTY(como::win::subspace* currentDesktop READ currentDesktop WRITE setCurrentDesktop
                   NOTIFY desktopChanged)
    Q_PROPERTY(QString currentActivity READ currentActivity NOTIFY currentActivityChanged)
    Q_PROPERTY(como::EffectWindow* activeWindow READ activeWindow WRITE activateWindow NOTIFY
                   windowActivated)
    Q_PROPERTY(QSize desktopGridSize READ desktopGridSize NOTIFY desktopGridSizeChanged)
    Q_PROPERTY(int desktopGridWidth READ desktopGridWidth NOTIFY desktopGridWidthChanged)
    Q_PROPERTY(int desktopGridHeight READ desktopGridHeight NOTIFY desktopGridHeightChanged)
    Q_PROPERTY(int workspaceWidth READ workspaceWidth)
    Q_PROPERTY(int workspaceHeight READ workspaceHeight)
    Q_PROPERTY(QList<como::win::subspace*> desktops READ desktops)
    Q_PROPERTY(bool optionRollOverDesktops READ optionRollOverDesktops)
    Q_PROPERTY(como::EffectScreen* activeScreen READ activeScreen)
    /**
     * Factor by which animation speed in the effect should be modified (multiplied).
     * If configurable in the effect itself, the option should have also 'default'
     * animation speed. The actual value should be determined using animationTime().
     * Note: The factor can be also 0, so make sure your code can cope with 0ms time
     * if used manually.
     */
    Q_PROPERTY(qreal animationTimeFactor READ animationTimeFactor)
    Q_PROPERTY(QList<como::EffectWindow*> stackingOrder READ stackingOrder)
    /**
     * Whether window decorations use the alpha channel.
     */
    Q_PROPERTY(bool decorationsHaveAlpha READ decorationsHaveAlpha)
    Q_PROPERTY(QPoint cursorPos READ cursorPos)
    Q_PROPERTY(QSize virtualScreenSize READ virtualScreenSize NOTIFY virtualScreenSizeChanged)
    Q_PROPERTY(
        QRect virtualScreenGeometry READ virtualScreenGeometry NOTIFY virtualScreenGeometryChanged)
    Q_PROPERTY(bool hasActiveFullScreenEffect READ hasActiveFullScreenEffect NOTIFY
                   hasActiveFullScreenEffectChanged)

    /**
     * The status of the session i.e if the user is logging out
     * @since 5.18
     */
    Q_PROPERTY(como::SessionState sessionState READ sessionState NOTIFY sessionStateChanged)

    friend class Effect;

public:
    using window = EffectWindow;
    using TouchBorderCallback
        = std::function<void(ElectricBorder border, const QSizeF&, EffectScreen* screen)>;

    explicit EffectsHandler();
    ~EffectsHandler() override;
    // for use by effects
    virtual void prePaintScreen(effect::screen_prepaint_data& data) = 0;
    virtual void paintScreen(effect::screen_paint_data& data) = 0;
    virtual void postPaintScreen() = 0;
    virtual void prePaintWindow(effect::window_prepaint_data& data) = 0;
    virtual void paintWindow(effect::window_paint_data& data) = 0;
    virtual void postPaintWindow(EffectWindow* w) = 0;
    virtual void drawWindow(effect::window_paint_data& data) = 0;
    virtual void buildQuads(EffectWindow* w, WindowQuadList& quadList) = 0;
    virtual QVariant kwinOption(KWinOption kwopt) = 0;
    /**
     * Sets the cursor while the mouse is intercepted.
     * @see startMouseInterception
     * @since 4.11
     */
    virtual void defineCursor(Qt::CursorShape shape) = 0;
    virtual QPoint cursorPos() const = 0;
    virtual bool grabKeyboard(Effect* effect) = 0;
    virtual void ungrabKeyboard() = 0;
    /**
     * Ensures that all mouse events are sent to the @p effect.
     * No window will get the mouse events. Only fullscreen effects providing a custom user
     * interface should be using this method. The input events are delivered to
     * Effect::windowInputMouseEvent.
     *
     * @note This method does not perform an X11 mouse grab. On X11 a fullscreen input window is
     * raised above all other windows, but no grab is performed.
     *
     * @param effect The effect
     * @param shape Sets the cursor to be used while the mouse is intercepted
     * @see stopMouseInterception
     * @see Effect::windowInputMouseEvent
     * @since 4.11
     */
    virtual void startMouseInterception(Effect* effect, Qt::CursorShape shape) = 0;
    /**
     * Releases the hold mouse interception for @p effect
     * @see startMouseInterception
     * @since 4.11
     */
    virtual void stopMouseInterception(Effect* effect) = 0;

    /**
     * @brief Registers a global shortcut with the provided @p action.
     *
     * @param shortcut The global shortcut which should trigger the action
     * @param action The action which gets triggered when the shortcut matches
     */
    virtual QList<QKeySequence> registerGlobalShortcut(QList<QKeySequence> const& shortcut,
                                                       QAction* action)
        = 0;
    virtual QList<QKeySequence>
    registerGlobalShortcutAndDefault(QList<QKeySequence> const& shortcut, QAction* action) = 0;
    /**
     * @brief Registers a global pointer shortcut with the provided @p action.
     *
     * @param modifiers The keyboard modifiers which need to be holded
     * @param pointerButtons The pointer buttons which need to be pressed
     * @param action The action which gets triggered when the shortcut matches
     */
    virtual void registerPointerShortcut(Qt::KeyboardModifiers modifiers,
                                         Qt::MouseButton pointerButtons,
                                         QAction* action)
        = 0;
    /**
     * @brief Registers a global axis shortcut with the provided @p action.
     *
     * @param modifiers The keyboard modifiers which need to be holded
     * @param axis The direction in which the axis needs to be moved
     * @param action The action which gets triggered when the shortcut matches
     */
    virtual void registerAxisShortcut(Qt::KeyboardModifiers modifiers,
                                      PointerAxisDirection axis,
                                      QAction* action)
        = 0;

    /**
     * @brief Registers a global touchpad swipe gesture shortcut with the provided @p action.
     *
     * @param direction The direction for the swipe
     * @param action The action which gets triggered when the gesture triggers
     * @since 5.10
     */
    virtual void registerTouchpadSwipeShortcut(SwipeDirection direction,
                                               uint fingerCount,
                                               QAction* action,
                                               std::function<void(qreal)> progressCallback)
        = 0;

    virtual void registerTouchpadPinchShortcut(PinchDirection direction,
                                               uint fingerCount,
                                               QAction* action,
                                               std::function<void(qreal)> progressCallback)
        = 0;

    /**
     * @brief Registers a global touchscreen swipe gesture shortcut with the provided @p action.
     *
     * @param direction The direction for the swipe
     * @param action The action which gets triggered when the gesture triggers
     * @since 5.25
     */
    virtual void registerTouchscreenSwipeShortcut(SwipeDirection direction,
                                                  uint fingerCount,
                                                  QAction* action,
                                                  std::function<void(qreal)> progressCallback)
        = 0;

    // Mouse polling
    virtual void startMousePolling() = 0;
    virtual void stopMousePolling() = 0;

    virtual void reserveElectricBorder(ElectricBorder border, Effect* effect) = 0;
    virtual void unreserveElectricBorder(ElectricBorder border, Effect* effect) = 0;

    /**
     * Registers the given @p action for the given @p border to be activated through
     * a touch swipe gesture.
     *
     * If the @p border gets triggered through a touch swipe gesture the QAction::triggered
     * signal gets invoked.
     *
     * To unregister the touch screen action either delete the @p action or
     * invoke unregisterTouchBorder.
     *
     * @see unregisterTouchBorder
     * @since 5.10
     */
    virtual void registerTouchBorder(ElectricBorder border, QAction* action) = 0;

    /**
     * Registers the given @p action for the given @p border to be activated through
     * a touch swipe gesture.
     *
     * If the @p border gets triggered through a touch swipe gesture the QAction::triggered
     * signal gets invoked.
     *
     * progressCallback will be dinamically called each time the touch position is updated
     * to show the effect "partially" activated
     *
     * To unregister the touch screen action either delete the @p action or
     * invoke unregisterTouchBorder.
     *
     * @see unregisterTouchBorder
     * @since 5.25
     */
    virtual void registerRealtimeTouchBorder(ElectricBorder border,
                                             QAction* action,
                                             TouchBorderCallback progressCallback)
        = 0;

    /**
     * Unregisters the given @p action for the given touch @p border.
     *
     * @see registerTouchBorder
     * @since 5.10
     */
    virtual void unregisterTouchBorder(ElectricBorder border, QAction* action) = 0;

    // functions that allow controlling windows/desktop
    virtual void activateWindow(como::EffectWindow* c) = 0;
    virtual como::EffectWindow* activeWindow() const = 0;
    Q_SCRIPTABLE virtual void
    moveWindow(como::EffectWindow* w, const QPoint& pos, bool snap = false, double snapAdjust = 1.0)
        = 0;

    /**
     * Moves a window to the given desktops
     * On X11, the window will end up on the last window in the list
     * Setting this to an empty list will set the window on all desktops
     *
     * @arg desktopIds a list of desktops the window should be placed on. NET::OnAllDesktops is not
     * a valid desktop X11Id
     */
    Q_SCRIPTABLE virtual void windowToDesktops(como::EffectWindow* w,
                                               QVector<win::subspace*> const& desktopIds)
        = 0;

    Q_SCRIPTABLE virtual void windowToScreen(como::EffectWindow* w, EffectScreen* screen) = 0;
    virtual void setShowingDesktop(bool showing) = 0;

    // Activities
    /**
     * @returns The ID of the current activity.
     */
    virtual QString currentActivity() const = 0;
    // Desktops
    /**
     * @returns The ID of the current desktop.
     */
    virtual win::subspace* currentDesktop() const = 0;
    /**
     * @returns Total number of desktops currently in existence.
     */
    virtual QList<win::subspace*> desktops() const = 0;
    /**
     * Set the current desktop to @a desktop.
     */
    virtual void setCurrentDesktop(win::subspace* subsp) = 0;
    /**
     * @returns The size of desktop layout in grid units.
     */
    virtual QSize desktopGridSize() const = 0;
    /**
     * @returns The width of desktop layout in grid units.
     */
    virtual int desktopGridWidth() const = 0;
    /**
     * @returns The height of desktop layout in grid units.
     */
    virtual int desktopGridHeight() const = 0;
    /**
     * @returns The width of desktop layout in pixels.
     */
    virtual int workspaceWidth() const = 0;
    /**
     * @returns The height of desktop layout in pixels.
     */
    virtual int workspaceHeight() const = 0;
    /**
     * @returns The ID of the desktop at the point @a coords or 0 if no desktop exists at that
     * point. @a coords is to be in grid units.
     */
    virtual win::subspace* desktopAtCoords(QPoint coords) const = 0;
    /**
     * @returns The coords of desktop @a id in grid units.
     */
    virtual QPoint desktopGridCoords(win::subspace* subsp) const = 0;
    /**
     * @returns The coords of the top-left corner of desktop @a id in pixels.
     */
    virtual QPoint desktopCoords(win::subspace* subsp) const = 0;
    /**
     * @returns The ID of the desktop above desktop @a id. Wraps around to the bottom of
     * the layout if @a wrap is set. If @a id is not set use the current one.
     */
    Q_SCRIPTABLE virtual como::win::subspace* desktopAbove(como::win::subspace* subsp = nullptr,
                                                           bool wrap = true) const
        = 0;
    /**
     * @returns The ID of the desktop to the right of desktop @a id. Wraps around to the
     * left of the layout if @a wrap is set. If @a id is not set use the current one.
     */
    Q_SCRIPTABLE virtual como::win::subspace* desktopToRight(como::win::subspace* subsp = nullptr,
                                                             bool wrap = true) const
        = 0;
    /**
     * @returns The ID of the desktop below desktop @a id. Wraps around to the top of the
     * layout if @a wrap is set. If @a id is not set use the current one.
     */
    Q_SCRIPTABLE virtual como::win::subspace* desktopBelow(como::win::subspace* subsp = nullptr,
                                                           bool wrap = true) const
        = 0;
    /**
     * @returns The ID of the desktop to the left of desktop @a id. Wraps around to the
     * right of the layout if @a wrap is set. If @a id is not set use the current one.
     */
    Q_SCRIPTABLE virtual como::win::subspace* desktopToLeft(como::win::subspace* subsp = nullptr,
                                                            bool wrap = true) const
        = 0;
    Q_SCRIPTABLE virtual QString desktopName(como::win::subspace* subsp) const = 0;
    virtual bool optionRollOverDesktops() const = 0;

    virtual EffectScreen* activeScreen() const = 0; // Xinerama
    virtual QRect
    clientArea(clientAreaOption, EffectScreen const* screen, win::subspace* subsp) const
        = 0;
    virtual QRect clientArea(clientAreaOption, const EffectWindow* c) const = 0;
    virtual QRect clientArea(clientAreaOption, const QPoint& p, win::subspace* subsp) const = 0;

    /**
     * The bounding size of all screens combined. Overlapping areas
     * are not counted multiple times.
     *
     * @see virtualScreenGeometry()
     * @see virtualScreenSizeChanged()
     * @since 5.0
     */
    virtual QSize virtualScreenSize() const = 0;
    /**
     * The bounding geometry of all outputs combined. Always starts at (0,0) and has
     * virtualScreenSize as it's size.
     *
     * @see virtualScreenSize()
     * @see virtualScreenGeometryChanged()
     * @since 5.0
     */
    virtual QRect virtualScreenGeometry() const = 0;
    /**
     * Factor by which animation speed in the effect should be modified (multiplied).
     * If configurable in the effect itself, the option should have also 'default'
     * animation speed. The actual value should be determined using animationTime().
     * Note: The factor can be also 0, so make sure your code can cope with 0ms time
     * if used manually.
     */
    virtual double animationTimeFactor() const = 0;
    virtual WindowQuadType newWindowQuadType() = 0;

    Q_SCRIPTABLE como::EffectWindow* findWindow(WId id) const;
    Q_SCRIPTABLE como::EffectWindow* findWindow(Wrapland::Server::Surface* surf) const;
    /**
     * Finds the EffectWindow for the internal window @p w.
     * If there is no such window @c null is returned.
     *
     * On Wayland this returns the internal window. On X11 it returns an Unamanged with the
     * window id matching that of the provided window @p w.
     *
     * @since 5.16
     */
    Q_SCRIPTABLE como::EffectWindow* findWindow(QWindow* w) const;
    /**
     * Finds the EffectWindow for the Toplevel with KWin internal @p id.
     * If there is no such window @c null is returned.
     *
     * @since 5.16
     */
    Q_SCRIPTABLE como::EffectWindow* findWindow(const QUuid& id) const;

    virtual QList<EffectWindow*> stackingOrder() const = 0;
    // window will be temporarily painted as if being at the top of the stack
    Q_SCRIPTABLE virtual void setElevatedWindow(como::EffectWindow* w, bool set) = 0;

    virtual void setTabBoxWindow(EffectWindow*) = 0;
    virtual QList<EffectWindow*> currentTabBoxWindowList() const = 0;
    virtual void refTabBox() = 0;
    virtual void unrefTabBox() = 0;
    virtual void closeTabBox() = 0;
    virtual EffectWindow* currentTabBoxWindow() const = 0;

    virtual void setActiveFullScreenEffect(Effect* e) = 0;
    virtual Effect* activeFullScreenEffect() const = 0;

    /**
     * Schedules the entire workspace to be repainted next time.
     * If you call it during painting (including prepaint) then it does not
     *  affect the current painting.
     */
    Q_SCRIPTABLE virtual void addRepaintFull() = 0;
    Q_SCRIPTABLE virtual void addRepaint(const QRect& r) = 0;
    Q_SCRIPTABLE virtual void addRepaint(const QRegion& r) = 0;
    Q_SCRIPTABLE virtual void addRepaint(int x, int y, int w, int h) = 0;

    Q_SCRIPTABLE virtual bool isEffectLoaded(const QString& name) const = 0;

    /**
     * @brief Whether the Compositor is OpenGL based (either GL 1 or 2).
     *
     * @return bool @c true in case of OpenGL based Compositor, @c false otherwise
     */
    virtual bool isOpenGLCompositing() const = 0;
    /**
     * @brief Provides access to the QPainter which is rendering to the back buffer.
     *
     * Only relevant for CompositingType QPainterCompositing. For all other compositing types
     * @c null is returned.
     *
     * @return QPainter* The Scene's QPainter or @c null.
     */
    virtual QPainter* scenePainter() = 0;
    virtual void reconfigure() = 0;

    virtual QByteArray readRootProperty(long atom, long type, int format) const = 0;

    /**
     * Returns @a true if the active window decoration has shadow API hooks.
     */
    virtual bool hasDecorationShadows() const = 0;

    /**
     * Returns @a true if the window decorations use the alpha channel, and @a false otherwise.
     * @since 4.5
     */
    virtual bool decorationsHaveAlpha() const = 0;

    /**
     * Creates a new frame object. If the frame does not have a static size
     * then it will be located at @a position with @a alignment. A
     * non-static frame will automatically adjust its size to fit the contents.
     * @returns A new @ref EffectFrame. It is the responsibility of the caller to delete the
     * EffectFrame.
     * @since 4.6
     */
    virtual std::unique_ptr<EffectFrame> effectFrame(EffectFrameStyle style,
                                                     bool staticSize = true,
                                                     const QPoint& position = QPoint(-1, -1),
                                                     Qt::Alignment alignment
                                                     = Qt::AlignCenter) const
        = 0;

    /**
     * Allows an effect to trigger a reload of itself.
     * This can be used by an effect which needs to be reloaded when screen geometry changes.
     * It is possible that the effect cannot be loaded again as it's supported method does no longer
     * hold.
     * @param effect The effect to reload
     * @since 4.8
     */
    virtual void reloadEffect(Effect* effect) = 0;

    /**
     * Whether the screen is currently considered as locked.
     * Note for technical reasons this is not always possible to detect. The screen will only
     * be considered as locked if the screen locking process implements the
     * org.freedesktop.ScreenSaver interface.
     *
     * @returns @c true if the screen is currently locked, @c false otherwise
     * @see screenLockingChanged
     * @since 4.11
     */
    virtual bool isScreenLocked() const = 0;

    /**
     * @brief Makes the OpenGL compositing context current.
     *
     * If the compositing backend is not using OpenGL, this method returns @c false.
     *
     * @return bool @c true if the context became current, @c false otherwise.
     */
    virtual bool makeOpenGLContextCurrent() = 0;
    /**
     * @brief Makes a null OpenGL context current resulting in no context
     * being current.
     *
     * If the compositing backend is not OpenGL based, this method is a noop.
     *
     * There is normally no reason for an Effect to call this method.
     */
    virtual void doneOpenGLContextCurrent() = 0;

    virtual xcb_connection_t* xcbConnection() const = 0;
    virtual uint32_t x11RootWindow() const = 0;

    /**
     * Interface to the Wayland display: this is relevant only
     * on Wayland, on X11 it will be nullptr
     * @since 5.5
     */
    virtual Wrapland::Server::Display* waylandDisplay() const = 0;

    /**
     * Whether animations are supported by the Scene.
     * If this method returns @c false Effects are supposed to not
     * animate transitions.
     *
     * @returns Whether the Scene can drive animations
     * @since 5.8
     */
    virtual bool animationsSupported() const = 0;

    /**
     * The current cursor image of the Platform.
     * @see cursorPos
     * @since 5.9
     */
    virtual effect::cursor_image cursorImage() const = 0;

    /**
     * The cursor image should be hidden.
     * @see showCursor
     * @since 5.9
     */
    virtual void hideCursor() = 0;

    /**
     * The cursor image should be shown again after having been hidden.
     * @see hideCursor
     * @since 5.9
     */
    virtual void showCursor() = 0;

    /**
     * @returns Whether or not the cursor is currently hidden
     */
    virtual bool isCursorHidden() const = 0;

    /**
     * Starts an interactive window selection process.
     *
     * Once the user selected a window the @p callback is invoked with the selected EffectWindow as
     * argument. In case the user cancels the interactive window selection or selecting a window is
     * currently not possible (e.g. screen locked) the @p callback is invoked with a @c nullptr
     * argument.
     *
     * During the interactive window selection the cursor is turned into a crosshair cursor.
     *
     * @param callback The function to invoke once the interactive window selection ends
     * @since 5.9
     */
    virtual void startInteractiveWindowSelection(std::function<void(como::EffectWindow*)> callback)
        = 0;

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
     * @since 5.9
     */
    virtual void startInteractivePositionSelection(std::function<void(const QPoint&)> callback) = 0;

    /**
     * Shows an on-screen-message. To hide it again use hideOnScreenMessage.
     *
     * @param message The message to show
     * @param iconName The optional themed icon name
     * @see hideOnScreenMessage
     * @since 5.9
     */
    virtual void showOnScreenMessage(const QString& message, const QString& iconName = QString())
        = 0;

    /**
     * Flags for how to hide a shown on-screen-message
     * @see hideOnScreenMessage
     * @since 5.9
     */
    enum class OnScreenMessageHideFlag {
        /**
         * The on-screen-message should skip the close window animation.
         * @see EffectWindow::skipsCloseAnimation
         */
        SkipsCloseAnimation = 1
    };
    Q_DECLARE_FLAGS(OnScreenMessageHideFlags, OnScreenMessageHideFlag)
    /**
     * Hides a previously shown on-screen-message again.
     * @param flags The flags for how to hide the message
     * @see showOnScreenMessage
     * @since 5.9
     */
    virtual void hideOnScreenMessage(OnScreenMessageHideFlags flags = OnScreenMessageHideFlags())
        = 0;

    /*
     * @returns The configuration used by the EffectsHandler.
     * @since 5.10
     */
    virtual KSharedConfigPtr config() const = 0;

    /**
     * @returns The global input configuration (kcminputrc)
     * @since 5.10
     */
    virtual KSharedConfigPtr inputConfig() const = 0;

    /**
     * Returns if activeFullScreenEffect is set
     */
    virtual bool hasActiveFullScreenEffect() const = 0;

    /**
     * Render the supplied OffscreenQuickView onto the scene
     * It can be called at any point during the scene rendering
     * @since 5.18
     */
    virtual void renderOffscreenQuickView(OffscreenQuickView* view) const = 0;

    /**
     * The status of the session i.e if the user is logging out
     * @since 5.18
     */
    virtual SessionState sessionState() const = 0;

    /**
     * Returns the list of all the screens connected to the system.
     */
    virtual QList<EffectScreen*> screens() const = 0;
    virtual EffectScreen* screenAt(const QPoint& point) const = 0;
    virtual EffectScreen* findScreen(const QString& name) const = 0;
    virtual EffectScreen* findScreen(int screenId) const = 0;

    virtual effect::region_integration& get_blur_integration() = 0;
    virtual effect::color_integration& get_contrast_integration() = 0;
    virtual effect::anim_integration& get_slide_integration() = 0;
    virtual effect::kscreen_integration& get_kscreen_integration() = 0;

    virtual QImage
    blit_from_framebuffer(effect::render_data& data, QRect const& geometry, double scale) const
        = 0;

    virtual QQmlEngine* qmlEngine() const = 0;

Q_SIGNALS:
    /**
     * This signal is emitted whenever a new @a screen is added to the system.
     */
    void screenAdded(como::EffectScreen* screen);
    /**
     * This signal is emitted whenever a @a screen is removed from the system.
     */
    void screenRemoved(como::EffectScreen* screen);
    /**
     * Signal emitted when the current desktop changed.
     * @param oldDesktop The previously current desktop
     * @param newDesktop The new current desktop
     * @param with The window which is taken over to the new desktop, can be NULL
     * @since 4.9
     */
    void desktopChanged(como::win::subspace* oldDesktop,
                        como::win::subspace* newDesktop,
                        como::EffectWindow* with);

    /**
     * Signal emmitted while desktop is changing for animation.
     * @param currentDesktop The current desktop untiotherwise.
     * @param offset The current desktop offset.
     * offset.x() = .6 means 60% of the way to the desktop to the right.
     * Positive Values means Up and Right.
     */
    void
    desktopChanging(como::win::subspace* currentDesktop, QPointF offset, como::EffectWindow* with);
    void desktopChangingCancelled();
    void desktopAdded(como::win::subspace* subsp);
    void desktopRemoved(como::win::subspace* subsp);

    /**
     * Emitted when the virtual desktop grid layout changes
     * @param size new size
     * @since 5.25
     */
    void desktopGridSizeChanged(const QSize& size);
    /**
     * Emitted when the virtual desktop grid layout changes
     * @param width new width
     * @since 5.25
     */
    void desktopGridWidthChanged(int width);
    /**
     * Emitted when the virtual desktop grid layout changes
     * @param height new height
     * @since 5.25
     */
    void desktopGridHeightChanged(int height);
    /**
     * Signal emitted when the desktop showing ("dashboard") state changed
     * The desktop is risen to the keepAbove layer, you may want to elevate
     * windows or such.
     * @since 5.3
     */
    void showingDesktopChanged(bool);
    /**
     * Signal emitted when a new window has been added to the Workspace.
     * @param w The added window
     * @since 4.7
     */
    void windowAdded(como::EffectWindow* w);
    /**
     * Signal emitted when a window is being removed from the Workspace.
     * An effect which wants to animate the window closing should connect
     * to this signal and reference the window by using
     * refWindow
     * @param w The window which is being closed
     * @since 4.7
     */
    void windowClosed(como::EffectWindow* w);
    /**
     * Signal emitted when a window get's activated.
     * @param w The new active window, or @c NULL if there is no active window.
     * @since 4.7
     */
    void windowActivated(como::EffectWindow* w);
    /**
     * Signal emitted when a window is deleted.
     * This means that a closed window is not referenced any more.
     * An effect bookkeeping the closed windows should connect to this
     * signal to clean up the internal references.
     * @param w The window which is going to be deleted.
     * @see EffectWindow::refWindow
     * @see EffectWindow::unrefWindow
     * @see windowClosed
     * @since 4.7
     */
    void windowDeleted(como::EffectWindow* w);
    /**
     * Signal emitted when a tabbox is added.
     * An effect who wants to replace the tabbox with itself should use refTabBox.
     * @param mode The TabBoxMode.
     * @see refTabBox
     * @see tabBoxClosed
     * @see tabBoxUpdated
     * @see tabBoxKeyEvent
     * @since 4.7
     */
    void tabBoxAdded(int mode);
    /**
     * Signal emitted when the TabBox was closed by KWin core.
     * An effect which referenced the TabBox should use unrefTabBox to unref again.
     * @see unrefTabBox
     * @see tabBoxAdded
     * @since 4.7
     */
    void tabBoxClosed();
    /**
     * Signal emitted when the selected TabBox window changed or the TabBox List changed.
     * An effect should only response to this signal if it referenced the TabBox with refTabBox.
     * @see refTabBox
     * @see currentTabBoxWindowList
     * @see currentTabBoxDesktopList
     * @see currentTabBoxWindow
     * @see currentTabBoxDesktop
     * @since 4.7
     */
    void tabBoxUpdated();
    /**
     * Signal emitted when a key event, which is not handled by TabBox directly is, happens while
     * TabBox is active. An effect might use the key event to e.g. change the selected window.
     * An effect should only response to this signal if it referenced the TabBox with refTabBox.
     * @param event The key event not handled by TabBox directly
     * @see refTabBox
     * @since 4.7
     */
    void tabBoxKeyEvent(QKeyEvent* event);
    /**
     * Signal emitted when mouse changed.
     * If an effect needs to get updated mouse positions, it needs to first call startMousePolling.
     * For a fullscreen effect it is better to use an input window and react on
     * windowInputMouseEvent.
     * @param pos The new mouse position
     * @param oldpos The previously mouse position
     * @param buttons The pressed mouse buttons
     * @param oldbuttons The previously pressed mouse buttons
     * @param modifiers Pressed keyboard modifiers
     * @param oldmodifiers Previously pressed keyboard modifiers.
     * @see startMousePolling
     * @since 4.7
     */
    void mouseChanged(const QPoint& pos,
                      const QPoint& oldpos,
                      Qt::MouseButtons buttons,
                      Qt::MouseButtons oldbuttons,
                      Qt::KeyboardModifiers modifiers,
                      Qt::KeyboardModifiers oldmodifiers);
    /**
     * Signal emitted when the cursor shape changed.
     * You'll likely want to query the current cursor as reaction:
     * xcb_xfixes_get_cursor_image_unchecked Connection to this signal is tracked, so if you don't
     * need it anymore, disconnect from it to stop cursor event filtering
     */
    void cursorShapeChanged();

    /**
     * Signal emitted after the screen geometry changed (e.g. add of a monitor).
     * Effects using displayWidth()/displayHeight() to cache information should
     * react on this signal and update the caches.
     * @param size The new screen size
     * @since 4.8
     */
    void screenGeometryChanged(const QSize& size);

    /**
     * This signal is emitted when the global
     * activity is changed
     * @param id id of the new current activity
     * @since 4.9
     */
    void currentActivityChanged(const QString& id);
    /**
     * This signal is emitted when a new activity is added
     * @param id id of the new activity
     * @since 4.9
     */
    void activityAdded(const QString& id);
    /**
     * This signal is emitted when the activity
     * is removed
     * @param id id of the removed activity
     * @since 4.9
     */
    void activityRemoved(const QString& id);
    /**
     * This signal is emitted when the screen got locked or unlocked.
     * @param locked @c true if the screen is now locked, @c false if it is now unlocked
     * @since 4.11
     */
    void screenLockingChanged(bool locked);

    /**
     * This signal is emitted just before the screen locker tries to grab keys and lock the screen
     * Effects should release any grabs immediately
     * @since 5.17
     */
    void screenAboutToLock();

    /**
     * This signels is emitted when ever the stacking order is change, ie. a window is risen
     * or lowered
     * @since 4.10
     */
    void stackingOrderChanged();
    /**
     * This signal is emitted when the user starts to approach the @p border with the mouse.
     * The @p factor describes how far away the mouse is in a relative mean. The values are in
     * [0.0, 1.0] with 0.0 being emitted when first entered and on leaving. The value 1.0 means that
     * the @p border is reached with the mouse. So the values are well suited for animations.
     * The signal is always emitted when the mouse cursor position changes.
     * @param border The screen edge which is being approached
     * @param factor Value in range [0.0,1.0] to describe how close the mouse is to the border
     * @param geometry The geometry of the edge which is being approached
     * @since 4.11
     */
    void screenEdgeApproaching(ElectricBorder border, qreal factor, const QRect& geometry);
    /**
     * Emitted whenever the virtualScreenSize changes.
     * @see virtualScreenSize()
     * @since 5.0
     */
    void virtualScreenSizeChanged();
    /**
     * Emitted whenever the virtualScreenGeometry changes.
     * @see virtualScreenGeometry()
     * @since 5.0
     */
    void virtualScreenGeometryChanged();
    /**
     * This signal gets emitted when the data on EffectWindow @p w for @p role changed.
     *
     * An Effect can connect to this signal to read the new value and react on it.
     * E.g. an Effect which does not operate on windows grabbed by another Effect wants
     * to cancel the already scheduled animation if another Effect adds a grab.
     *
     * @param w The EffectWindow for which the data changed
     * @param role The data role which changed
     * @see EffectWindow::setData
     * @see EffectWindow::data
     * @since 5.8.4
     */
    void windowDataChanged(como::EffectWindow* w, int role);

    /**
     * This signal is emitted when active fullscreen effect changed.
     *
     * @see activeFullScreenEffect
     * @see setActiveFullScreenEffect
     * @since 5.14
     */
    void activeFullScreenEffectChanged();

    /**
     * This signal is emitted when active fullscreen effect changed to being
     * set or unset
     *
     * @see activeFullScreenEffect
     * @see setActiveFullScreenEffect
     * @since 5.15
     */
    void hasActiveFullScreenEffectChanged();

    /**
     * This signal is emitted when the session state was changed
     * @since 5.18
     */
    void sessionStateChanged();

    void startupAdded(const QString& id, const QIcon& icon);
    void startupChanged(const QString& id, const QIcon& icon);
    void startupRemoved(const QString& id);

    void frameRendered(como::effect::screen_paint_data& data);

    void globalShortcutChanged(QAction* action, QKeySequence const& seq);

protected:
    virtual EffectWindow* find_window_by_wid(WId id) const = 0;
    virtual EffectWindow* find_window_by_surface(Wrapland::Server::Surface* surface) const = 0;
    virtual EffectWindow* find_window_by_qwindow(QWindow* window) const = 0;
    virtual EffectWindow* find_window_by_uuid(QUuid const& id) const = 0;

    QVector<EffectPair> loaded_effects;
    // QHash< QString, EffectFactory* > effect_factories;
    bool sw_composite;
};

/**
 * Pointer to the global EffectsHandler object.
 */
extern COMO_EXPORT EffectsHandler* effects;

}
