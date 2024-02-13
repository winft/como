/*
SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "abstractplatformcontext.h"

#include "eglhelpers.h"
#include <como/render/gl/egl_context_attribute_builder.h>

#include <logging.h>

#include <QOpenGLContext>

#include <memory>

namespace como
{

namespace QPA
{

AbstractPlatformContext::AbstractPlatformContext(QOpenGLContext* context,
                                                 EGLDisplay display,
                                                 EGLConfig config)
    : QPlatformOpenGLContext()
    , m_eglDisplay(display)
    , m_config(config ? config : configFromFormat(m_eglDisplay, context->format()))
    , m_format(formatFromConfig(m_eglDisplay, m_config))
{
}

AbstractPlatformContext::~AbstractPlatformContext()
{
    if (m_context != EGL_NO_CONTEXT) {
        eglDestroyContext(m_eglDisplay, m_context);
    }
}

void AbstractPlatformContext::doneCurrent()
{
    eglMakeCurrent(m_eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
}

QSurfaceFormat AbstractPlatformContext::format() const
{
    return m_format;
}

QFunctionPointer AbstractPlatformContext::getProcAddress(const char* procName)
{
    return eglGetProcAddress(procName);
}

bool AbstractPlatformContext::isValid() const
{
    return m_context != EGL_NO_CONTEXT;
}

bool AbstractPlatformContext::bindApi()
{
    if (eglBindAPI(isOpenGLES() ? EGL_OPENGL_ES_API : EGL_OPENGL_API) == EGL_FALSE) {
        qCWarning(KWIN_QPA) << "eglBindAPI failed";
        return false;
    }
    return true;
}

void AbstractPlatformContext::createContext(EGLContext shareContext)
{
    const QByteArray eglExtensions = eglQueryString(eglDisplay(), EGL_EXTENSIONS);
    const QList<QByteArray> extensions = eglExtensions.split(' ');
    const bool haveRobustness
        = extensions.contains(QByteArrayLiteral("EGL_EXT_create_context_robustness"));
    const bool haveCreateContext = extensions.contains(QByteArrayLiteral("EGL_KHR_create_context"));
    const bool haveContextPriority
        = extensions.contains(QByteArrayLiteral("EGL_IMG_context_priority"));

    std::vector<std::unique_ptr<render::gl::context_attribute_builder>> candidates;
    if (isOpenGLES()) {
        if (haveCreateContext && haveRobustness && haveContextPriority) {
            auto glesRobustPriority
                = std::make_unique<render::gl::egl_gles_context_attribute_builder>();
            glesRobustPriority->setVersion(2);
            glesRobustPriority->setRobust(true);
            glesRobustPriority->setHighPriority(true);
            candidates.push_back(std::move(glesRobustPriority));
        }
        if (haveCreateContext && haveRobustness) {
            auto glesRobust = std::make_unique<render::gl::egl_gles_context_attribute_builder>();
            glesRobust->setVersion(2);
            glesRobust->setRobust(true);
            candidates.push_back(std::move(glesRobust));
        }
        if (haveContextPriority) {
            auto glesPriority = std::make_unique<render::gl::egl_gles_context_attribute_builder>();
            glesPriority->setVersion(2);
            glesPriority->setHighPriority(true);
            candidates.push_back(std::move(glesPriority));
        }
        auto gles = std::make_unique<render::gl::egl_gles_context_attribute_builder>();
        gles->setVersion(2);
        candidates.push_back(std::move(gles));
    } else {
        // Try to create a 3.1 core context
        if (m_format.majorVersion() >= 3 && haveCreateContext) {
            if (haveRobustness && haveContextPriority) {
                auto robustCorePriority
                    = std::make_unique<render::gl::egl_context_attribute_builder>();
                robustCorePriority->setVersion(m_format.majorVersion(), m_format.minorVersion());
                robustCorePriority->setRobust(true);
                robustCorePriority->setForwardCompatible(true);
                if (m_format.profile() == QSurfaceFormat::CoreProfile) {
                    robustCorePriority->setCoreProfile(true);
                } else if (m_format.profile() == QSurfaceFormat::CompatibilityProfile) {
                    robustCorePriority->setCompatibilityProfile(true);
                }
                robustCorePriority->setHighPriority(true);
                candidates.push_back(std::move(robustCorePriority));
            }
            if (haveRobustness) {
                auto robustCore = std::make_unique<render::gl::egl_context_attribute_builder>();
                robustCore->setVersion(m_format.majorVersion(), m_format.minorVersion());
                robustCore->setRobust(true);
                robustCore->setForwardCompatible(true);
                if (m_format.profile() == QSurfaceFormat::CoreProfile) {
                    robustCore->setCoreProfile(true);
                } else if (m_format.profile() == QSurfaceFormat::CompatibilityProfile) {
                    robustCore->setCompatibilityProfile(true);
                }
                candidates.push_back(std::move(robustCore));
            }
            if (haveContextPriority) {
                auto corePriority = std::make_unique<render::gl::egl_context_attribute_builder>();
                corePriority->setVersion(m_format.majorVersion(), m_format.minorVersion());
                corePriority->setForwardCompatible(true);
                if (m_format.profile() == QSurfaceFormat::CoreProfile) {
                    corePriority->setCoreProfile(true);
                } else if (m_format.profile() == QSurfaceFormat::CompatibilityProfile) {
                    corePriority->setCompatibilityProfile(true);
                }
                corePriority->setHighPriority(true);
                candidates.push_back(std::move(corePriority));
            }
            auto core = std::make_unique<render::gl::egl_context_attribute_builder>();
            core->setVersion(m_format.majorVersion(), m_format.minorVersion());
            core->setForwardCompatible(true);
            if (m_format.profile() == QSurfaceFormat::CoreProfile) {
                core->setCoreProfile(true);
            } else if (m_format.profile() == QSurfaceFormat::CompatibilityProfile) {
                core->setCompatibilityProfile(true);
            }
            candidates.push_back(std::move(core));
        }
        if (haveRobustness && haveCreateContext && haveContextPriority) {
            auto robustPriority = std::make_unique<render::gl::egl_context_attribute_builder>();
            robustPriority->setRobust(true);
            robustPriority->setHighPriority(true);
            candidates.push_back(std::move(robustPriority));
        }
        if (haveRobustness && haveCreateContext) {
            auto robust = std::make_unique<render::gl::egl_context_attribute_builder>();
            robust->setRobust(true);
            candidates.push_back(std::move(robust));
        }
        candidates.emplace_back(new render::gl::egl_context_attribute_builder);
    }

    EGLContext context = EGL_NO_CONTEXT;
    for (auto it = candidates.begin(); it != candidates.end(); it++) {
        const auto attribs = (*it)->build();
        context = eglCreateContext(eglDisplay(), config(), shareContext, attribs.data());
        if (context != EGL_NO_CONTEXT) {
            qCDebug(KWIN_QPA) << "Created EGL context with attributes:" << (*it).get();
            break;
        }
    }

    if (context == EGL_NO_CONTEXT) {
        qCWarning(KWIN_QPA) << "Failed to create EGL context";
        return;
    }
    m_context = context;
}

}
}
