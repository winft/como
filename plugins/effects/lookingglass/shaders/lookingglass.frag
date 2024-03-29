// SPDX-FileCopyrightText: 2023 Roman Gilg <subdiff@gmail.com>
//
// SPDX-License-Identifier: GPL-2.0-or-later

uniform sampler2D sampler;
uniform vec2 u_cursor;
uniform float u_zoom;
uniform float u_radius;
uniform vec2 u_textureSize;

varying vec2 texcoord0;

#define PI 3.14159

void main()
{
    vec2 d = u_cursor - texcoord0;
    float dist = sqrt(d.x*d.x + d.y*d.y);
    vec2 texcoord = texcoord0;
    if (dist < u_radius) {
        float disp = sin(dist / u_radius * PI) * (u_zoom - 1.0) * 20.0;
        texcoord += d / dist * disp;
    }

    texcoord = texcoord/u_textureSize;
    texcoord.t = 1.0 - texcoord.t;
    gl_FragColor = texture2D(sampler, texcoord);
}

