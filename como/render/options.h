/*
    SPDX-FileCopyrightText: 2023 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "types.h"
#include "x11/types.h"

#include "como_export.h"
#include <como/base/types.h>

#include <KConfigWatcher>

namespace como::render
{

class settings;

class COMO_EXPORT options_qobject : public QObject
{
    Q_OBJECT

public:
    options_qobject(base::operation_mode mode);

    bool sw_compositing() const;

    // Separate to mode so the user can toggle
    bool isUseCompositing() const;

    x11::hidden_preview hiddenPreviews() const
    {
        return m_hiddenPreviews;
    }

    qint64 maxFpsInterval() const
    {
        return m_maxFpsInterval;
    }

    uint refreshRate() const
    {
        return m_refreshRate;
    }
    qint64 vBlankTime() const
    {
        return m_vBlankTime;
    }
    bool isGlStrictBinding() const
    {
        return m_glStrictBinding;
    }
    bool isGlStrictBindingFollowsDriver() const
    {
        return m_glStrictBindingFollowsDriver;
    }

    bool windowsBlockCompositing() const
    {
        return m_windowsBlockCompositing;
    }

    render::animation_curve animationCurve() const
    {
        return m_animationCurve;
    }

    // setters
    void set_sw_compositing(bool sw);
    void setUseCompositing(bool useCompositing);
    void setHiddenPreviews(x11::hidden_preview hiddenPreviews);
    void setMaxFpsInterval(qint64 maxFpsInterval);
    void setRefreshRate(uint refreshRate);
    void setVBlankTime(qint64 vBlankTime);
    void setGlStrictBinding(bool glStrictBinding);
    void setGlStrictBindingFollowsDriver(bool glStrictBindingFollowsDriver);
    void setWindowsBlockCompositing(bool set);
    void setAnimationCurve(render::animation_curve curve);

    static bool defaultUseCompositing()
    {
        return true;
    }
    static x11::hidden_preview defaultHiddenPreviews()
    {
        return x11::hidden_preview::shown;
    }
    static qint64 defaultMaxFpsInterval()
    {
        return (1 * 1000 * 1000 * 1000) / 60.0; // nanoseconds / Hz
    }
    static int defaultMaxFps()
    {
        return 60;
    }
    static uint defaultRefreshRate()
    {
        return 0;
    }
    static uint defaultVBlankTime()
    {
        return 6000; // 6ms
    }
    static bool defaultGlStrictBinding()
    {
        return true;
    }
    static bool defaultGlStrictBindingFollowsDriver()
    {
        return true;
    }

    base::operation_mode windowing_mode;

Q_SIGNALS:
    // for properties
    void sw_compositing_changed();
    void useCompositingChanged();
    void maxFpsIntervalChanged();
    void refreshRateChanged();
    void vBlankTimeChanged();
    void glStrictBindingChanged();
    void glStrictBindingFollowsDriverChanged();
    void hiddenPreviewsChanged();
    void windowsBlockCompositingChanged();
    void animationSpeedChanged();
    void animationCurveChanged();

    void configChanged();

private:
    bool m_sw_compositing{false};
    bool m_useCompositing{defaultUseCompositing()};
    x11::hidden_preview m_hiddenPreviews{defaultHiddenPreviews()};
    qint64 m_maxFpsInterval{defaultMaxFpsInterval()};

    // Settings that should be auto-detected
    uint m_refreshRate{defaultRefreshRate()};
    qint64 m_vBlankTime{defaultVBlankTime()};
    bool m_glStrictBinding{defaultGlStrictBinding()};
    bool m_glStrictBindingFollowsDriver{defaultGlStrictBindingFollowsDriver()};
    bool m_windowsBlockCompositing{true};
    render::animation_curve m_animationCurve{render::animation_curve::linear};

    friend class options;
};

class COMO_EXPORT options
{
public:
    using hidden_preview_t = x11::hidden_preview;

    options(base::operation_mode mode, KSharedConfigPtr config);
    ~options();

    void updateSettings();

    void reloadCompositingSettings(bool force = false);

    /**
     * Performs loading all settings except compositing related.
     */
    void loadConfig();
    /**
     * Performs loading of compositing settings which do not depend on OpenGL.
     */
    bool loadCompositingConfig(bool force);

    /**
     * Returns the animation time factor for desktop effects.
     */
    double animationTimeFactor() const;

    std::unique_ptr<options_qobject> qobject;

private:
    void syncFromKcfgc();

    QScopedPointer<settings> m_settings;
    KConfigWatcher::Ptr m_configWatcher;
};

}
