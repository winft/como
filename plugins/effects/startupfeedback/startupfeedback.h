/*
    SPDX-FileCopyrightText: 2010 Martin Gräßlin <mgraesslin@kde.org>

SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef KWIN_STARTUPFEEDBACK_H
#define KWIN_STARTUPFEEDBACK_H

#include <render/effect/interface/effect.h>

#include <KConfigWatcher>
#include <KStartupInfo>
#include <QObject>

#include <chrono>

class KSelectionOwner;
namespace como
{
class GLShader;
class GLTexture;

class StartupFeedbackEffect : public Effect
{
    Q_OBJECT
    Q_PROPERTY(int type READ type)
public:
    StartupFeedbackEffect();
    ~StartupFeedbackEffect() override;

    void reconfigure(ReconfigureFlags flags) override;
    void prePaintScreen(effect::screen_prepaint_data& data) override;
    void paintScreen(effect::screen_paint_data& data) override;
    void postPaintScreen() override;
    bool isActive() const override;

    int requestedEffectChainPosition() const override
    {
        return 90;
    }

    int type() const
    {
        return int(m_type);
    }

    static bool supported();

private Q_SLOTS:
    void gotNewStartup(const QString& id, const QIcon& icon);
    void gotRemoveStartup(const QString& id);
    void gotStartupChange(const QString& id, const QIcon& icon);
    void slotMouseChanged(const QPoint& pos,
                          const QPoint& oldpos,
                          Qt::MouseButtons buttons,
                          Qt::MouseButtons oldbuttons,
                          Qt::KeyboardModifiers modifiers,
                          Qt::KeyboardModifiers oldmodifiers);

private:
    enum FeedbackType { NoFeedback, BouncingFeedback, BlinkingFeedback, PassiveFeedback };

    struct Startup {
        QIcon icon;
        QSharedPointer<QTimer> expiredTimer;
    };

    void start(const Startup& startup);
    void stop();
    QImage scalePixmap(const QPixmap& pm, const QSize& size) const;
    void prepareTextures(const QPixmap& pix);
    QRect feedbackRect() const;

    qreal m_bounceSizesRatio;
    KStartupInfo* m_startupInfo;
    KSelectionOwner* m_selection;
    QString m_currentStartup;
    QMap<QString, Startup> m_startups;
    bool m_active;
    int m_frame;
    int m_progress;
    std::chrono::milliseconds m_lastPresentTime;
    std::unique_ptr<GLTexture> m_bouncingTextures[5];
    std::unique_ptr<GLTexture> m_texture; // for passive and blinking
    FeedbackType m_type;
    QRect m_currentGeometry, m_dirtyRect;
    std::unique_ptr<GLShader> m_blinkingShader;
    int m_cursorSize;
    KConfigWatcher::Ptr m_configWatcher;
    bool m_splashVisible;
    std::chrono::seconds m_timeout;
};
} // namespace

#endif
