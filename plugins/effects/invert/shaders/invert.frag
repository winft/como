// SPDX-FileCopyrightText: 2023 Roman Gilg <subdiff@gmail.com>
//
// SPDX-License-Identifier: GPL-2.0-or-later

uniform sampler2D sampler;
uniform vec4 modulation;
uniform float saturation;

varying vec2 texcoord0;

void main()
{
    vec4 tex = texture2D(sampler, texcoord0);

    if (saturation != 1.0) {
        vec3 desaturated = tex.rgb * vec3( 0.30, 0.59, 0.11 );
        desaturated = vec3( dot( desaturated, tex.rgb ));
        tex.rgb = tex.rgb * vec3( saturation ) + desaturated * vec3( 1.0 - saturation );
    }

    tex.rgb = vec3(1.0) - tex.rgb;
    tex *= modulation;
    tex.rgb *= tex.a;

    gl_FragColor = tex;
}
