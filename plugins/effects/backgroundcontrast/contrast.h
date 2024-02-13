/*
    SPDX-FileCopyrightText: 2010 Fredrik Höglund <fredrik@kde.org>
    SPDX-FileCopyrightText: 2014 Marco Martin <mart@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef CONTRAST_H
#define CONTRAST_H

#include <como/render/effect/interface/effect.h>
#include <como/render/gl/interface/framebuffer.h>
#include <como/render/gl/interface/vertex_buffer.h>

#include <QVector2D>
#include <QVector>
#include <memory>
#include <unordered_map>

namespace como
{

class ContrastShader;

class ContrastEffect : public como::Effect
{
    Q_OBJECT
public:
    ContrastEffect();
    ~ContrastEffect() override;

    static bool supported();
    static bool enabledByDefault();

    void reconfigure(ReconfigureFlags flags) override;
    void drawWindow(effect::window_paint_data& data) override;

    bool provides(Feature feature) override;
    bool isActive() const override;

    int requestedEffectChainPosition() const override
    {
        return 21;
    }

    void slotWindowDeleted(como::EffectWindow* w);
    void reset();

    struct Data {
        QMatrix4x4 colorMatrix;
        QRegion contrastRegion;
        std::unique_ptr<GLTexture> texture;
        std::unique_ptr<GLFramebuffer> fbo;
    };
    std::unordered_map<EffectWindow const*, Data> m_windowData;

private:
    QRegion contrastRegion(const EffectWindow* w) const;
    bool shouldContrast(effect::window_paint_data const& data) const;
    void doContrast(effect::window_paint_data& data, QRegion const& shape);
    void uploadRegion(std::span<QVector2D> map, const QRegion& region);
    void uploadGeometry(GLVertexBuffer* vbo, const QRegion& region);

private:
    std::unique_ptr<ContrastShader> shader;
};

inline bool ContrastEffect::provides(Effect::Feature feature)
{
    if (feature == Contrast) {
        return true;
    }
    return como::Effect::provides(feature);
}

}

#endif
