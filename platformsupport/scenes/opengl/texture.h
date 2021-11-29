/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2006 Lubos Lunak <l.lunak@kde.org>
Copyright (C) 2009, 2010, 2011 Martin Gräßlin <mgraesslin@kde.org>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/
#pragma once

#include <kwingltexture.h>
#include <kwingltexture_p.h>

namespace KWin
{
class WindowPixmap;

namespace render::gl
{
class backend;
class texture_private;

class texture : public GLTexture
{
public:
    explicit texture(gl::backend* backend);
    ~texture() override;

    texture& operator=(texture const& tex);

    void discard() override final;

private:
    texture(texture_private& dd);

    bool load(WindowPixmap* pixmap);
    void updateFromPixmap(WindowPixmap* pixmap);

    inline texture_private* d_func()
    {
        return reinterpret_cast<texture_private*>(qGetPtrHelper(d_ptr));
    }
    inline const texture_private* d_func() const
    {
        return reinterpret_cast<texture_private const*>(qGetPtrHelper(d_ptr));
    }

    friend class texture_private;
    friend class window_pixmap;
};

class texture_private : public GLTexturePrivate
{
public:
    ~texture_private() override;

    virtual bool loadTexture(WindowPixmap* pixmap) = 0;
    virtual void updateTexture(WindowPixmap* pixmap);
    virtual gl::backend* backend() = 0;

protected:
    texture_private();

private:
    Q_DISABLE_COPY(texture_private)
};

}
}
