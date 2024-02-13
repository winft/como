#version 140
// SPDX-FileCopyrightText: 2023 Roman Gilg <subdiff@gmail.com>
//
// SPDX-License-Identifier: GPL-2.0-or-later

uniform sampler2D sampler;
uniform vec2 offsets[16];
uniform vec4 kernel[16];

in vec2 texcoord0;
out vec4 fragColor;

void main(void)
{
    vec4 sum = texture(sampler, texcoord0.st) * kernel[0];
    for (int i = 1; i < 16; i++) {
        sum += texture(sampler, texcoord0.st - offsets[i]) * kernel[i];
        sum += texture(sampler, texcoord0.st + offsets[i]) * kernel[i];
    }
    fragColor = sum;
}

