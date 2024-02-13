/*
    SPDX-FileCopyrightText: 2007 Rivo Laks <rivolaks@hot.ee>
    SPDX-FileCopyrightText: 2011 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2012 Philipp Knechtges <philipp-dev@knechtges.com>
    SPDX-FileCopyrightText: 2022 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "texture.h"

#include "platform.h"
#include "utils.h"
#include "utils_funcs.h"

#include "texture_p.h"

#include <como/render/effect/interface/paint_data.h>
#include <como/render/effect/interface/types.h>
#include <como/render/gl/interface/framebuffer.h>
#include <como/render/gl/interface/vertex_buffer.h>

#include <QImage>
#include <QMatrix4x4>
#include <QPixmap>
#include <QVector2D>
#include <QVector3D>
#include <QVector4D>

namespace como
{

//****************************************
// GLTexture
//****************************************

bool GLTexturePrivate::s_supportsFramebufferObjects = false;
bool GLTexturePrivate::s_supportsARGB32 = false;
bool GLTexturePrivate::s_supportsUnpack = false;
bool GLTexturePrivate::s_supportsTextureStorage = false;
bool GLTexturePrivate::s_supportsTextureSwizzle = false;
bool GLTexturePrivate::s_supportsTextureFormatRG = false;
bool GLTexturePrivate::s_supportsTexture16Bit = false;
uint GLTexturePrivate::s_textureObjectCounter = 0;
uint GLTexturePrivate::s_fbo = 0;

// Table of GL formats/types associated with different values of QImage::Format.
// Zero values indicate a direct upload is not feasible.
//
// Note: Blending is set up to expect premultiplied data, so the non-premultiplied
// Format_ARGB32 must be converted to Format_ARGB32_Premultiplied ahead of time.
struct {
    GLenum internalFormat;
    GLenum format;
    GLenum type;
} static const formatTable[] = {
    {0, 0, 0},                                              // QImage::Format_Invalid
    {0, 0, 0},                                              // QImage::Format_Mono
    {0, 0, 0},                                              // QImage::Format_MonoLSB
    {0, 0, 0},                                              // QImage::Format_Indexed8
    {GL_RGB8, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV},        // QImage::Format_RGB32
    {0, 0, 0},                                              // QImage::Format_ARGB32
    {GL_RGBA8, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV},       // QImage::Format_ARGB32_Premultiplied
    {GL_RGB8, GL_BGR, GL_UNSIGNED_SHORT_5_6_5_REV},         // QImage::Format_RGB16
    {0, 0, 0},                                              // QImage::Format_ARGB8565_Premultiplied
    {0, 0, 0},                                              // QImage::Format_RGB666
    {0, 0, 0},                                              // QImage::Format_ARGB6666_Premultiplied
    {GL_RGB5, GL_BGRA, GL_UNSIGNED_SHORT_1_5_5_5_REV},      // QImage::Format_RGB555
    {0, 0, 0},                                              // QImage::Format_ARGB8555_Premultiplied
    {GL_RGB8, GL_RGB, GL_UNSIGNED_BYTE},                    // QImage::Format_RGB888
    {GL_RGB4, GL_BGRA, GL_UNSIGNED_SHORT_4_4_4_4_REV},      // QImage::Format_RGB444
    {GL_RGBA4, GL_BGRA, GL_UNSIGNED_SHORT_4_4_4_4_REV},     // QImage::Format_ARGB4444_Premultiplied
    {GL_RGB8, GL_RGBA, GL_UNSIGNED_BYTE},                   // QImage::Format_RGBX8888
    {0, 0, 0},                                              // QImage::Format_RGBA8888
    {GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE},                  // QImage::Format_RGBA8888_Premultiplied
    {GL_RGB10, GL_RGBA, GL_UNSIGNED_INT_2_10_10_10_REV},    // QImage::Format_BGR30
    {GL_RGB10_A2, GL_RGBA, GL_UNSIGNED_INT_2_10_10_10_REV}, // QImage::Format_A2BGR30_Premultiplied
    {GL_RGB10, GL_BGRA, GL_UNSIGNED_INT_2_10_10_10_REV},    // QImage::Format_RGB30
    {GL_RGB10_A2, GL_BGRA, GL_UNSIGNED_INT_2_10_10_10_REV}, // QImage::Format_A2RGB30_Premultiplied
    {GL_R8, GL_RED, GL_UNSIGNED_BYTE},                      // QImage::Format_Alpha8
    {GL_R8, GL_RED, GL_UNSIGNED_BYTE},                      // QImage::Format_Grayscale8
    {GL_RGBA16, GL_RGBA, GL_UNSIGNED_SHORT},                // QImage::Format_RGBX64
    {0, 0, 0},                                              // QImage::Format_RGBA64
    {GL_RGBA16, GL_RGBA, GL_UNSIGNED_SHORT},                // QImage::Format_RGBA64_Premultiplied
    {GL_R16, GL_RED, GL_UNSIGNED_SHORT},                    // QImage::Format_Grayscale16
    {0, 0, 0},                                              // QImage::Format_BGR888
};

GLTexture::GLTexture()
    : d_ptr{std::make_unique<GLTexturePrivate>()}
{
}

GLTexture::GLTexture(std::unique_ptr<GLTexturePrivate> impl)
    : d_ptr{std::move(impl)}
{
}

GLTexture::GLTexture(GLTexture&& tex) = default;

GLTexture::GLTexture(const QImage& image, GLenum target)
    : GLTexture()
{
    if (image.isNull()) {
        return;
    }

    d_ptr->m_target = target;

    if (d_ptr->m_target != GL_TEXTURE_RECTANGLE_ARB) {
        d_ptr->m_scale.setWidth(1.0 / image.width());
        d_ptr->m_scale.setHeight(1.0 / image.height());
    } else {
        d_ptr->m_scale.setWidth(1.0);
        d_ptr->m_scale.setHeight(1.0);
    }

    d_ptr->m_size = image.size();
    set_content_transform(effect::transform_type::flipped_180);
    d_ptr->m_canUseMipmaps = false;
    d_ptr->m_mipLevels = 1;

    d_ptr->updateMatrix();

    glGenTextures(1, &d_ptr->m_texture);
    bind();

    if (!GLPlatform::instance()->isGLES()) {
        QImage im;
        GLenum internalFormat;
        GLenum format;
        GLenum type;

        const QImage::Format index = image.format();

        if (index < sizeof(formatTable) / sizeof(formatTable[0])
            && formatTable[index].internalFormat
            && !(formatTable[index].type == GL_UNSIGNED_SHORT && !d_ptr->s_supportsTexture16Bit)) {
            internalFormat = formatTable[index].internalFormat;
            format = formatTable[index].format;
            type = formatTable[index].type;
            im = image;
        } else {
            im = image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
            internalFormat = GL_RGBA8;
            format = GL_BGRA;
            type = GL_UNSIGNED_INT_8_8_8_8_REV;
        }

        d_ptr->m_internalFormat = internalFormat;

        if (d_ptr->s_supportsTextureStorage) {
            glTexStorage2D(d_ptr->m_target, 1, internalFormat, im.width(), im.height());
            glTexSubImage2D(
                d_ptr->m_target, 0, 0, 0, im.width(), im.height(), format, type, im.constBits());
            d_ptr->m_immutable = true;
        } else {
            glTexParameteri(d_ptr->m_target, GL_TEXTURE_MAX_LEVEL, d_ptr->m_mipLevels - 1);
            glTexImage2D(d_ptr->m_target,
                         0,
                         internalFormat,
                         im.width(),
                         im.height(),
                         0,
                         format,
                         type,
                         im.constBits());
        }
    } else {
        d_ptr->m_internalFormat = GL_RGBA8;

        if (d_ptr->s_supportsARGB32) {
            const QImage im = image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
            glTexImage2D(d_ptr->m_target,
                         0,
                         GL_BGRA_EXT,
                         im.width(),
                         im.height(),
                         0,
                         GL_BGRA_EXT,
                         GL_UNSIGNED_BYTE,
                         im.constBits());
        } else {
            const QImage im = image.convertToFormat(QImage::Format_RGBA8888_Premultiplied);
            glTexImage2D(d_ptr->m_target,
                         0,
                         GL_RGBA,
                         im.width(),
                         im.height(),
                         0,
                         GL_RGBA,
                         GL_UNSIGNED_BYTE,
                         im.constBits());
        }
    }

    unbind();
    setFilter(GL_LINEAR);
}

GLTexture::GLTexture(const QPixmap& pixmap, GLenum target)
    : GLTexture(pixmap.toImage(), target)
{
}

GLTexture::GLTexture(const QString& fileName)
    : GLTexture(QImage(fileName))
{
}

GLTexture::GLTexture(GLenum internalFormat, int width, int height, int levels)
    : GLTexture()
{
    d_ptr->m_target = GL_TEXTURE_2D;
    d_ptr->m_scale.setWidth(1.0 / width);
    d_ptr->m_scale.setHeight(1.0 / height);
    d_ptr->m_size = QSize(width, height);
    d_ptr->m_canUseMipmaps = levels > 1;
    d_ptr->m_mipLevels = levels;
    d_ptr->m_filter = levels > 1 ? GL_NEAREST_MIPMAP_LINEAR : GL_NEAREST;

    d_ptr->updateMatrix();

    glGenTextures(1, &d_ptr->m_texture);
    bind();

    if (!GLPlatform::instance()->isGLES()) {
        if (d_ptr->s_supportsTextureStorage) {
            glTexStorage2D(d_ptr->m_target, levels, internalFormat, width, height);
            d_ptr->m_immutable = true;
        } else {
            glTexParameteri(d_ptr->m_target, GL_TEXTURE_MAX_LEVEL, levels - 1);
            glTexImage2D(d_ptr->m_target,
                         0,
                         internalFormat,
                         width,
                         height,
                         0,
                         GL_BGRA,
                         GL_UNSIGNED_INT_8_8_8_8_REV,
                         nullptr);
        }
        d_ptr->m_internalFormat = internalFormat;
    } else {
        // The format parameter in glTexSubImage() must match the internal format
        // of the texture, so it's important that we allocate the texture with
        // the format that will be used in update() and clear().
        const GLenum format = d_ptr->s_supportsARGB32 ? GL_BGRA_EXT : GL_RGBA;
        glTexImage2D(
            d_ptr->m_target, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, nullptr);

        // This is technically not true, but it means that code that calls
        // internalFormat() won't need to be specialized for GLES2.
        d_ptr->m_internalFormat = GL_RGBA8;
    }

    unbind();
}

GLTexture::GLTexture(GLenum internalFormat, const QSize& size, int levels)
    : GLTexture(internalFormat, size.width(), size.height(), levels)
{
}

GLTexture::GLTexture(GLuint textureId, GLenum internalFormat, const QSize& size, int levels)
    : GLTexture()
{
    d_ptr->m_foreign = true;
    d_ptr->m_texture = textureId;
    d_ptr->m_target = GL_TEXTURE_2D;
    d_ptr->m_scale.setWidth(1.0 / size.width());
    d_ptr->m_scale.setHeight(1.0 / size.height());
    d_ptr->m_size = size;
    d_ptr->m_canUseMipmaps = levels > 1;
    d_ptr->m_mipLevels = levels;
    d_ptr->m_filter = levels > 1 ? GL_NEAREST_MIPMAP_LINEAR : GL_NEAREST;
    d_ptr->m_internalFormat = internalFormat;

    d_ptr->updateMatrix();
}

GLTexture::~GLTexture() = default;

GLTexture& GLTexture::operator=(GLTexture&& tex) = default;

GLTexturePrivate::GLTexturePrivate()
    : m_texture(0)
    , m_target(0)
    , m_internalFormat(0)
    , m_filter(GL_NEAREST)
    , m_wrapMode(GL_REPEAT)
    , m_canUseMipmaps(false)
    , m_markedDirty(false)
    , m_filterChanged(true)
    , m_wrapModeChanged(false)
    , m_immutable(false)
    , m_foreign(false)
    , m_mipLevels(1)
    , m_unnormalizeActive(0)
    , m_normalizeActive(0)
{
    ++s_textureObjectCounter;
}

GLTexturePrivate::~GLTexturePrivate()
{
    if (m_texture != 0 && !m_foreign) {
        glDeleteTextures(1, &m_texture);
    }
    // Delete the FBO if this is the last Texture
    if (--s_textureObjectCounter == 0 && s_fbo) {
        glDeleteFramebuffers(1, &s_fbo);
        s_fbo = 0;
    }
}

void GLTexturePrivate::initStatic()
{
    if (!GLPlatform::instance()->isGLES()) {
        s_supportsFramebufferObjects = hasGLVersion(3, 0)
            || hasGLExtension("GL_ARB_framebuffer_object")
            || hasGLExtension(QByteArrayLiteral("GL_EXT_framebuffer_object"));
        s_supportsTextureStorage
            = hasGLVersion(4, 2) || hasGLExtension(QByteArrayLiteral("GL_ARB_texture_storage"));
        s_supportsTextureSwizzle
            = hasGLVersion(3, 3) || hasGLExtension(QByteArrayLiteral("GL_ARB_texture_swizzle"));
        // see https://www.opengl.org/registry/specs/ARB/texture_rg.txt
        s_supportsTextureFormatRG
            = hasGLVersion(3, 0) || hasGLExtension(QByteArrayLiteral("GL_ARB_texture_rg"));
        s_supportsTexture16Bit = true;
        s_supportsARGB32 = true;
        s_supportsUnpack = true;
    } else {
        s_supportsFramebufferObjects = true;
        s_supportsTextureStorage
            = hasGLVersion(3, 0) || hasGLExtension(QByteArrayLiteral("GL_EXT_texture_storage"));
        s_supportsTextureSwizzle = hasGLVersion(3, 0);
        // see https://www.khronos.org/registry/gles/extensions/EXT/EXT_texture_rg.txt
        s_supportsTextureFormatRG
            = hasGLVersion(3, 0) || hasGLExtension(QByteArrayLiteral("GL_EXT_texture_rg"));
        s_supportsTexture16Bit = hasGLExtension(QByteArrayLiteral("GL_EXT_texture_norm16"));

        // QImage::Format_ARGB32_Premultiplied is a packed-pixel format, so it's only
        // equivalent to GL_BGRA/GL_UNSIGNED_BYTE on little-endian systems.
        s_supportsARGB32 = QSysInfo::ByteOrder == QSysInfo::LittleEndian
            && hasGLExtension(QByteArrayLiteral("GL_EXT_texture_format_BGRA8888"));

        s_supportsUnpack = hasGLExtension(QByteArrayLiteral("GL_EXT_unpack_subimage"));
    }
}

void GLTexturePrivate::cleanup()
{
    s_supportsFramebufferObjects = false;
    s_supportsARGB32 = false;
}

bool GLTexture::isNull() const
{
    return GL_NONE == d_ptr->m_texture;
}

QSize GLTexture::size() const
{
    return d_ptr->m_size;
}

void GLTexture::update(const QImage& image, const QPoint& offset, const QRect& src)
{
    if (image.isNull() || isNull()) {
        return;
    }

    Q_ASSERT(!d_ptr->m_foreign);

    GLenum glFormat;
    GLenum type;
    QImage::Format uploadFormat;
    if (!GLPlatform::instance()->isGLES()) {
        const QImage::Format index = image.format();

        if (index < sizeof(formatTable) / sizeof(formatTable[0])
            && formatTable[index].internalFormat
            && !(formatTable[index].type == GL_UNSIGNED_SHORT && !d_ptr->s_supportsTexture16Bit)) {
            glFormat = formatTable[index].format;
            type = formatTable[index].type;
            uploadFormat = index;
        } else {
            glFormat = GL_BGRA;
            type = GL_UNSIGNED_INT_8_8_8_8_REV;
            uploadFormat = QImage::Format_ARGB32_Premultiplied;
        }
    } else {
        if (d_ptr->s_supportsARGB32) {
            glFormat = GL_BGRA_EXT;
            type = GL_UNSIGNED_BYTE;
            uploadFormat = QImage::Format_ARGB32_Premultiplied;
        } else {
            glFormat = GL_RGBA;
            type = GL_UNSIGNED_BYTE;
            uploadFormat = QImage::Format_RGBA8888_Premultiplied;
        }
    }
    bool useUnpack = d_ptr->s_supportsUnpack && image.format() == uploadFormat && !src.isNull();

    QImage im;
    if (useUnpack) {
        im = image;
        Q_ASSERT(im.depth() % 8 == 0);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, im.bytesPerLine() / (im.depth() / 8));
        glPixelStorei(GL_UNPACK_SKIP_PIXELS, src.x());
        glPixelStorei(GL_UNPACK_SKIP_ROWS, src.y());
    } else {
        if (src.isNull()) {
            im = image;
        } else {
            im = image.copy(src);
        }
        if (im.format() != uploadFormat) {
            im.convertTo(uploadFormat);
        }
    }

    int width = image.width();
    int height = image.height();
    if (!src.isNull()) {
        width = src.width();
        height = src.height();
    }

    bind();

    glTexSubImage2D(
        d_ptr->m_target, 0, offset.x(), offset.y(), width, height, glFormat, type, im.constBits());

    unbind();

    if (useUnpack) {
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
        glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
        glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
    }
}

void GLTexture::discard()
{
    d_ptr = std::make_unique<GLTexturePrivate>();
}

void GLTexture::bind()
{
    glBindTexture(d_ptr->m_target, d_ptr->m_texture);

    if (d_ptr->m_markedDirty) {
        d_ptr->onDamage();
    }
    if (d_ptr->m_filterChanged) {
        GLenum minFilter = GL_NEAREST;
        GLenum magFilter = GL_NEAREST;

        switch (d_ptr->m_filter) {
        case GL_NEAREST:
            minFilter = magFilter = GL_NEAREST;
            break;

        case GL_LINEAR:
            minFilter = magFilter = GL_LINEAR;
            break;

        case GL_NEAREST_MIPMAP_NEAREST:
        case GL_NEAREST_MIPMAP_LINEAR:
            magFilter = GL_NEAREST;
            minFilter = d_ptr->m_canUseMipmaps ? d_ptr->m_filter : GL_NEAREST;
            break;

        case GL_LINEAR_MIPMAP_NEAREST:
        case GL_LINEAR_MIPMAP_LINEAR:
            magFilter = GL_LINEAR;
            minFilter = d_ptr->m_canUseMipmaps ? d_ptr->m_filter : GL_LINEAR;
            break;
        }

        glTexParameteri(d_ptr->m_target, GL_TEXTURE_MIN_FILTER, minFilter);
        glTexParameteri(d_ptr->m_target, GL_TEXTURE_MAG_FILTER, magFilter);

        d_ptr->m_filterChanged = false;
    }
    if (d_ptr->m_wrapModeChanged) {
        glTexParameteri(d_ptr->m_target, GL_TEXTURE_WRAP_S, d_ptr->m_wrapMode);
        glTexParameteri(d_ptr->m_target, GL_TEXTURE_WRAP_T, d_ptr->m_wrapMode);
        d_ptr->m_wrapModeChanged = false;
    }
}

void GLTexture::generateMipmaps()
{
    if (d_ptr->m_canUseMipmaps && d_ptr->s_supportsFramebufferObjects)
        glGenerateMipmap(d_ptr->m_target);
}

void GLTexture::unbind()
{
    glBindTexture(d_ptr->m_target, 0);
}

void GLTexture::render(QSize const& target_size)
{
    if (target_size.isEmpty()) {
        // nothing to paint. m_vbo is likely nullptr and cacheed size empty as well, #337090
        return;
    }

    d_ptr->update_cache({{}, d_ptr->get_buffer_size()}, target_size);
    d_ptr->m_vbo->render(GL_TRIANGLE_STRIP);
}

void GLTexture::render(effect::render_data const& data,
                       QRegion const& region,
                       QSize const& target_size)
{
    render(data, {{}, d_ptr->get_buffer_size()}, region, target_size);
}

void GLTexture::render(effect::render_data const& data,
                       QRect const& source,
                       QRegion const& region,
                       QSize const& target_size)
{
    if (target_size.isEmpty()) {
        // nothing to paint. m_vbo is likely nullptr and cacheed size empty as well, #337090
        return;
    }

    d_ptr->update_cache(source, target_size);
    d_ptr->m_vbo->render(data, region, GL_TRIANGLE_STRIP);
}

GLuint GLTexture::texture() const
{
    return d_ptr->m_texture;
}

GLenum GLTexture::target() const
{
    return d_ptr->m_target;
}

GLenum GLTexture::filter() const
{
    return d_ptr->m_filter;
}

GLenum GLTexture::internalFormat() const
{
    return d_ptr->m_internalFormat;
}

void GLTexture::clear()
{
    Q_ASSERT(!d_ptr->m_foreign);
    if (!GLTexturePrivate::s_fbo && GLFramebuffer::supported()
        && GLPlatform::instance()->driver() != Driver_Catalyst) // fail. -> bug #323065
        glGenFramebuffers(1, &GLTexturePrivate::s_fbo);

    if (GLTexturePrivate::s_fbo) {
        // Clear the texture
        GLuint previousFramebuffer = 0;
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, reinterpret_cast<GLint*>(&previousFramebuffer));
        if (GLTexturePrivate::s_fbo != previousFramebuffer)
            glBindFramebuffer(GL_FRAMEBUFFER, GLTexturePrivate::s_fbo);
        glClearColor(0, 0, 0, 0);
        glFramebufferTexture2D(
            GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, d_ptr->m_texture, 0);
        glClear(GL_COLOR_BUFFER_BIT);
        if (GLTexturePrivate::s_fbo != previousFramebuffer)
            glBindFramebuffer(GL_FRAMEBUFFER, previousFramebuffer);
    } else {
        if (const int size = width() * height()) {
            std::vector<uint32_t> buffer(size, 0);
            bind();
            if (!GLPlatform::instance()->isGLES()) {
                glTexSubImage2D(GL_TEXTURE_2D,
                                0,
                                0,
                                0,
                                width(),
                                height(),
                                GL_BGRA,
                                GL_UNSIGNED_INT_8_8_8_8_REV,
                                buffer.data());
            } else {
                const GLenum format = d_ptr->s_supportsARGB32 ? GL_BGRA_EXT : GL_RGBA;
                glTexSubImage2D(GL_TEXTURE_2D,
                                0,
                                0,
                                0,
                                width(),
                                height(),
                                format,
                                GL_UNSIGNED_BYTE,
                                buffer.data());
            }
            unbind();
        }
    }
}

bool GLTexture::isDirty() const
{
    return d_ptr->m_markedDirty;
}

void GLTexture::setFilter(GLenum filter)
{
    if (filter != d_ptr->m_filter) {
        d_ptr->m_filter = filter;
        d_ptr->m_filterChanged = true;
    }
}

void GLTexture::setWrapMode(GLenum mode)
{
    if (mode != d_ptr->m_wrapMode) {
        d_ptr->m_wrapMode = mode;
        d_ptr->m_wrapModeChanged = true;
    }
}

QSize GLTexturePrivate::get_buffer_size() const
{
    return m_textureToBufferMatrix.mapRect(QRect(QPoint(), m_size)).size();
}

void GLTexturePrivate::onDamage()
{
    // No-op
}

void GLTexture::setDirty()
{
    d_ptr->m_markedDirty = true;
}

void GLTexturePrivate::updateMatrix()
{
    auto flip = [this] { m_textureToBufferMatrix.scale(-1, 1); };
    auto rotate = [this](auto angle) { m_textureToBufferMatrix.rotate(angle, 0, 0, 1); };

    m_textureToBufferMatrix.setToIdentity();
    switch (m_textureToBufferTransform) {
    case effect::transform_type::rotated_90:
        rotate(90);
        break;
    case effect::transform_type::rotated_180:
        rotate(180);
        break;
    case effect::transform_type::rotated_270:
        rotate(270);
        break;
    case effect::transform_type::flipped:
        flip();
        break;
    case effect::transform_type::flipped_90:
        flip();
        rotate(90);
        break;
    case effect::transform_type::flipped_180:
        flip();
        rotate(180);
        break;
    case effect::transform_type::flipped_270:
        flip();
        rotate(270);
        break;
    case effect::transform_type::normal:
    default:
        break;
    }

    m_matrix[NormalizedCoordinates].setToIdentity();
    m_matrix[UnnormalizedCoordinates].setToIdentity();

    if (m_target == GL_TEXTURE_RECTANGLE_ARB)
        m_matrix[NormalizedCoordinates].scale(m_size.width(), m_size.height());
    else
        m_matrix[UnnormalizedCoordinates].scale(1.0 / m_size.width(), 1.0 / m_size.height());

    m_matrix[NormalizedCoordinates].translate(0.5, 0.5);
    m_matrix[NormalizedCoordinates] *= m_textureToBufferMatrix;

    // our Y axis is flipped vs OpenGL
    m_matrix[NormalizedCoordinates].scale(1, -1);
    m_matrix[NormalizedCoordinates].translate(-0.5, -0.5);

    m_matrix[UnnormalizedCoordinates].translate(m_size.width() / 2, m_size.height() / 2);
    m_matrix[UnnormalizedCoordinates] *= m_textureToBufferMatrix;
    m_matrix[UnnormalizedCoordinates].scale(1, -1);
    m_matrix[UnnormalizedCoordinates].translate(-m_size.width() / 2, -m_size.height() / 2);
}

void GLTexturePrivate::update_cache(QRect const& source, QSize const& size)
{
    if (cache.size == size && cache.source != source) {
        return;
    }

    cache.size = size;
    cache.source = source;

    float const texWidth = (m_target == GL_TEXTURE_RECTANGLE_ARB) ? m_size.width() : 1.0f;
    float const texHeight = (m_target == GL_TEXTURE_RECTANGLE_ARB) ? m_size.height() : 1.0f;

    auto const rotated_size = get_buffer_size();

    QMatrix4x4 textureMat;
    textureMat.translate(texWidth / 2, texHeight / 2);
    textureMat *= m_textureToBufferMatrix;

    // our Y axis is flipped vs OpenGL
    textureMat.scale(1, -1);
    textureMat.translate(-texWidth / 2, -texHeight / 2);
    textureMat.scale(texWidth / rotated_size.width(), texHeight / rotated_size.height());

    auto const p1 = textureMat.map(QPointF(source.x(), source.y()));
    auto const p2 = textureMat.map(QPointF(source.x(), source.y() + source.height()));
    auto const p3 = textureMat.map(QPointF(source.x() + source.width(), source.y()));
    auto const p4
        = textureMat.map(QPointF(source.x() + source.width(), source.y() + source.height()));

    if (!m_vbo) {
        m_vbo = std::make_unique<GLVertexBuffer>(GLVertexBuffer::Static);
    }

    const std::array<GLVertex2D, 4> data{
        GLVertex2D{
            .position = QVector2D(0, 0),
            .texcoord = QVector2D(p1),
        },
        GLVertex2D{
            .position = QVector2D(0, size.height()),
            .texcoord = QVector2D(p2),
        },
        GLVertex2D{
            .position = QVector2D(size.width(), 0),
            .texcoord = QVector2D(p3),
        },
        GLVertex2D{
            .position = QVector2D(size.width(), size.height()),
            .texcoord = QVector2D(p4),
        },
    };

    m_vbo->setVertices(data);
}

void GLTexture::set_content_transform(effect::transform_type transform)
{
    if (d_ptr->m_textureToBufferTransform == transform) {
        return;
    }
    d_ptr->m_textureToBufferTransform = transform;
    d_ptr->updateMatrix();
}

effect::transform_type GLTexture::get_content_transform() const
{
    return d_ptr->m_textureToBufferTransform;
}

QMatrix4x4 GLTexture::get_content_transform_matrix() const
{
    return d_ptr->m_textureToBufferMatrix;
}

void GLTexture::setSwizzle(GLenum red, GLenum green, GLenum blue, GLenum alpha)
{
    if (!GLPlatform::instance()->isGLES()) {
        const GLuint swizzle[] = {red, green, blue, alpha};
        glTexParameteriv(
            d_ptr->m_target, GL_TEXTURE_SWIZZLE_RGBA, reinterpret_cast<const GLint*>(swizzle));
    } else {
        glTexParameteri(d_ptr->m_target, GL_TEXTURE_SWIZZLE_R, red);
        glTexParameteri(d_ptr->m_target, GL_TEXTURE_SWIZZLE_G, green);
        glTexParameteri(d_ptr->m_target, GL_TEXTURE_SWIZZLE_B, blue);
        glTexParameteri(d_ptr->m_target, GL_TEXTURE_SWIZZLE_A, alpha);
    }
}

int GLTexture::width() const
{
    return d_ptr->m_size.width();
}

int GLTexture::height() const
{
    return d_ptr->m_size.height();
}

QMatrix4x4 GLTexture::matrix(TextureCoordinateType type) const
{
    return d_ptr->m_matrix[type];
}

bool GLTexture::framebufferObjectSupported()
{
    return GLTexturePrivate::s_supportsFramebufferObjects;
}

bool GLTexture::supportsSwizzle()
{
    return GLTexturePrivate::s_supportsTextureSwizzle;
}

bool GLTexture::supportsFormatRG()
{
    return GLTexturePrivate::s_supportsTextureFormatRG;
}

QImage GLTexture::toImage() const
{
    if (target() != GL_TEXTURE_2D) {
        return QImage();
    }
    QImage ret(size(), QImage::Format_RGBA8888_Premultiplied);

    GLint currentTextureBinding;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &currentTextureBinding);

    if (currentTextureBinding != static_cast<int>(texture())) {
        glBindTexture(GL_TEXTURE_2D, texture());
    }
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8_REV, ret.bits());
    if (currentTextureBinding != static_cast<int>(texture())) {
        glBindTexture(GL_TEXTURE_2D, currentTextureBinding);
    }
    return ret;
}

}
