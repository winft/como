/*
SPDX-FileCopyrightText: 2017 Martin Flöser <mgraesslin@kde.org>

SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "context_attribute_builder.h"

#include "como_export.h"

namespace como::render::gl
{

class COMO_EXPORT egl_context_attribute_builder : public context_attribute_builder
{
public:
    std::vector<int> build() const override;
};

class COMO_EXPORT egl_gles_context_attribute_builder : public context_attribute_builder
{
public:
    std::vector<int> build() const override;
};

}
