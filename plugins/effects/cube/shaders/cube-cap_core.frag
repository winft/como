#version 140
// SPDX-FileCopyrightText: 2023 Roman Gilg <subdiff@gmail.com>
//
// SPDX-License-Identifier: GPL-2.0-or-later

uniform sampler2D sampler;
uniform vec4 geometryColor;
uniform float u_opacity;
uniform int u_mirror;
uniform int u_untextured;

in vec2 texcoord0;

out vec4 fragColor;

vec2 mirrorTex(vec2 coords) {
    vec2 mirrored = coords;
    if (u_mirror != 0) {
        mirrored.t = mirrored.t * (-1.0) + 1.0;
    }
    return mirrored;
}

void main() {
    vec4 color = geometryColor;
    vec2 texCoord = mirrorTex(texcoord0);
    vec4 tex = texture(sampler, texCoord);
    if (texCoord.s < 0.0 || texCoord.s > 1.0 ||
            texCoord.t < 0.0 || texCoord.t > 1.0 || u_untextured != 0) {
        tex = geometryColor;
    }
    color.rgb = tex.rgb*tex.a + color.rgb*(1.0-tex.a);
    color.a = u_opacity;

    fragColor = color;
}
