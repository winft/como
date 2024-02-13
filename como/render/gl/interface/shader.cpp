/*
    SPDX-FileCopyrightText: 2007 Rivo Laks <rivolaks@hot.ee>
    SPDX-FileCopyrightText: 2011 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2022 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "shader.h"

#include "platform.h"

#include <como/base/logging.h>
#include <como/render/effect/interface/paint_data.h>
#include <como/render/effect/interface/types.h>
#include <como/render/gl/interface/utils.h>

#include <QFile>

namespace como
{

GLShader::GLShader(unsigned int flags)
    : mValid(false)
    , mLocationsResolved(false)
    , mExplicitLinking(flags & ExplicitLinking)
{
    mProgram = glCreateProgram();
}

GLShader::GLShader(const QString& vertexfile, const QString& fragmentfile, unsigned int flags)
    : mValid(false)
    , mLocationsResolved(false)
    , mExplicitLinking(flags & ExplicitLinking)
{
    mProgram = glCreateProgram();
    loadFromFiles(vertexfile, fragmentfile);
}

GLShader::~GLShader()
{
    if (mProgram) {
        glDeleteProgram(mProgram);
    }
}

bool GLShader::loadFromFiles(const QString& vertexFile, const QString& fragmentFile)
{
    QFile vf(vertexFile);
    if (!vf.open(QIODevice::ReadOnly)) {
        qCCritical(KWIN_CORE) << "Couldn't open" << vertexFile << "for reading!";
        return false;
    }
    const QByteArray vertexSource = vf.readAll();

    QFile ff(fragmentFile);
    if (!ff.open(QIODevice::ReadOnly)) {
        qCCritical(KWIN_CORE) << "Couldn't open" << fragmentFile << "for reading!";
        return false;
    }
    const QByteArray fragmentSource = ff.readAll();

    return load(vertexSource, fragmentSource);
}

bool GLShader::link()
{
    // Be optimistic
    mValid = true;

    glLinkProgram(mProgram);

    // Get the program info log
    int maxLength, length;
    glGetProgramiv(mProgram, GL_INFO_LOG_LENGTH, &maxLength);

    QByteArray log(maxLength, 0);
    glGetProgramInfoLog(mProgram, maxLength, &length, log.data());

    // Make sure the program linked successfully
    int status;
    glGetProgramiv(mProgram, GL_LINK_STATUS, &status);

    if (status == 0) {
        qCCritical(KWIN_CORE) << "Failed to link shader:"
                              << "\n"
                              << log;
        mValid = false;
    } else if (length > 0) {
        qCDebug(KWIN_CORE) << "Shader link log:" << log;
    }

    return mValid;
}

const QByteArray GLShader::prepareSource(GLenum shaderType, const QByteArray& source) const
{
    Q_UNUSED(shaderType)
    // Prepare the source code
    QByteArray ba;
    if (GLPlatform::instance()->isGLES()
        && GLPlatform::instance()->glslVersion() < kVersionNumber(3, 0)) {
        ba.append("precision highp float;\n");
    }
    ba.append(source);
    if (GLPlatform::instance()->isGLES()
        && GLPlatform::instance()->glslVersion() >= kVersionNumber(3, 0)) {
        ba.replace("#version 140", "#version 300 es\n\nprecision highp float;\n");
    }

    return ba;
}

bool GLShader::compile(GLuint program, GLenum shaderType, const QByteArray& source) const
{
    GLuint shader = glCreateShader(shaderType);

    QByteArray preparedSource = prepareSource(shaderType, source);
    const char* src = preparedSource.constData();
    glShaderSource(shader, 1, &src, nullptr);

    // Compile the shader
    glCompileShader(shader);

    // Get the shader info log
    int maxLength, length;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &maxLength);

    QByteArray log(maxLength, 0);
    glGetShaderInfoLog(shader, maxLength, &length, log.data());

    // Check the status
    int status;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);

    if (status == 0) {
        const char* typeName = (shaderType == GL_VERTEX_SHADER ? "vertex" : "fragment");
        qCCritical(KWIN_CORE) << "Failed to compile" << typeName << "shader:"
                              << "\n"
                              << log;
    } else if (length > 0)
        qCDebug(KWIN_CORE) << "Shader compile log:" << log;

    if (status != 0)
        glAttachShader(program, shader);

    glDeleteShader(shader);
    return status != 0;
}

bool GLShader::load(const QByteArray& vertexSource, const QByteArray& fragmentSource)
{
    mValid = false;

    // Compile the vertex shader
    if (!vertexSource.isEmpty()) {
        bool success = compile(mProgram, GL_VERTEX_SHADER, vertexSource);

        if (!success)
            return false;
    }

    // Compile the fragment shader
    if (!fragmentSource.isEmpty()) {
        bool success = compile(mProgram, GL_FRAGMENT_SHADER, fragmentSource);

        if (!success)
            return false;
    }

    if (mExplicitLinking)
        return true;

    // link() sets mValid
    return link();
}

void GLShader::bindAttributeLocation(const char* name, int index)
{
    glBindAttribLocation(mProgram, index, name);
}

void GLShader::bindFragDataLocation(const char* name, int index)
{
    if (!GLPlatform::instance()->isGLES()
        && (hasGLVersion(3, 0) || hasGLExtension(QByteArrayLiteral("GL_EXT_gpu_shader4"))))
        glBindFragDataLocation(mProgram, index, name);
}

void GLShader::bind()
{
    glUseProgram(mProgram);
}

void GLShader::unbind()
{
    glUseProgram(0);
}

void GLShader::resolveLocations()
{
    if (mLocationsResolved)
        return;

    mMatrixLocation[ModelViewProjectionMatrix] = uniformLocation("modelViewProjectionMatrix");

    mVec2Location[Offset] = uniformLocation("offset");

    mVec4Location[ModulationConstant] = uniformLocation("modulation");

    mFloatLocation[Saturation] = uniformLocation("saturation");

    mColorLocation[Color] = uniformLocation("geometryColor");

    mIntLocation[TextureWidth] = uniformLocation("textureWidth");
    mIntLocation[TextureHeight] = uniformLocation("textureHeight");

    mLocationsResolved = true;
}

int GLShader::uniformLocation(const char* name)
{
    const int location = glGetUniformLocation(mProgram, name);
    return location;
}

bool GLShader::setUniform(GLShader::MatrixUniform uniform, const QMatrix4x4& matrix)
{
    resolveLocations();
    return setUniform(mMatrixLocation[uniform], matrix);
}

bool GLShader::setUniform(GLShader::Vec2Uniform uniform, const QVector2D& value)
{
    resolveLocations();
    return setUniform(mVec2Location[uniform], value);
}

bool GLShader::setUniform(GLShader::Vec4Uniform uniform, const QVector4D& value)
{
    resolveLocations();
    return setUniform(mVec4Location[uniform], value);
}

bool GLShader::setUniform(GLShader::FloatUniform uniform, float value)
{
    resolveLocations();
    return setUniform(mFloatLocation[uniform], value);
}

bool GLShader::setUniform(GLShader::IntUniform uniform, int value)
{
    resolveLocations();
    return setUniform(mIntLocation[uniform], value);
}

bool GLShader::setUniform(GLShader::ColorUniform uniform, const QVector4D& value)
{
    resolveLocations();
    return setUniform(mColorLocation[uniform], value);
}

bool GLShader::setUniform(GLShader::ColorUniform uniform, const QColor& value)
{
    resolveLocations();
    return setUniform(mColorLocation[uniform], value);
}

bool GLShader::setUniform(const char* name, float value)
{
    const int location = uniformLocation(name);
    return setUniform(location, value);
}

bool GLShader::setUniform(const char* name, int value)
{
    const int location = uniformLocation(name);
    return setUniform(location, value);
}

bool GLShader::setUniform(const char* name, const QVector2D& value)
{
    const int location = uniformLocation(name);
    return setUniform(location, value);
}

bool GLShader::setUniform(const char* name, const QVector3D& value)
{
    const int location = uniformLocation(name);
    return setUniform(location, value);
}

bool GLShader::setUniform(const char* name, const QVector4D& value)
{
    const int location = uniformLocation(name);
    return setUniform(location, value);
}

bool GLShader::setUniform(const char* name, const QMatrix4x4& value)
{
    const int location = uniformLocation(name);
    return setUniform(location, value);
}

bool GLShader::setUniform(const char* name, const QColor& color)
{
    const int location = uniformLocation(name);
    return setUniform(location, color);
}

bool GLShader::setUniform(int location, float value)
{
    if (location >= 0) {
        glUniform1f(location, value);
    }
    return (location >= 0);
}

bool GLShader::setUniform(int location, int value)
{
    if (location >= 0) {
        glUniform1i(location, value);
    }
    return (location >= 0);
}

bool GLShader::setUniform(int location, const QVector2D& value)
{
    if (location >= 0) {
        glUniform2fv(location, 1, reinterpret_cast<const GLfloat*>(&value));
    }
    return (location >= 0);
}

bool GLShader::setUniform(int location, const QVector3D& value)
{
    if (location >= 0) {
        glUniform3fv(location, 1, reinterpret_cast<const GLfloat*>(&value));
    }
    return (location >= 0);
}

bool GLShader::setUniform(int location, const QVector4D& value)
{
    if (location >= 0) {
        glUniform4fv(location, 1, reinterpret_cast<const GLfloat*>(&value));
    }
    return (location >= 0);
}

bool GLShader::setUniform(int location, const QMatrix4x4& value)
{
    if (location >= 0) {
        glUniformMatrix4fv(location, 1, GL_FALSE, value.constData());
    }
    return (location >= 0);
}

bool GLShader::setUniform(int location, const QColor& color)
{
    if (location >= 0) {
        glUniform4f(location, color.redF(), color.greenF(), color.blueF(), color.alphaF());
    }
    return (location >= 0);
}

int GLShader::attributeLocation(const char* name)
{
    int location = glGetAttribLocation(mProgram, name);
    return location;
}

bool GLShader::setAttribute(const char* name, float value)
{
    int location = attributeLocation(name);
    if (location >= 0) {
        glVertexAttrib1f(location, value);
    }
    return (location >= 0);
}

QMatrix4x4 GLShader::getUniformMatrix4x4(const char* name)
{
    int location = uniformLocation(name);
    if (location >= 0) {
        GLfloat m[16];
        glGetnUniformfv(mProgram, location, sizeof(m), m);
        // clang-format off
        QMatrix4x4 matrix(m[0], m[4], m[8],  m[12],
                          m[1], m[5], m[9],  m[13],
                          m[2], m[6], m[10], m[14],
                          m[3], m[7], m[11], m[15]);
        // clang-format on
        matrix.optimize();
        return matrix;
    } else {
        return QMatrix4x4();
    }
}

}
