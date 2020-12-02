/*
 * Copyright (C) 2017 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef SHADER_H
#define SHADER_H

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <GLES3/gl3.h>
#include <GLES3/gl3ext.h>

#include <utils/Log.h>

const char gVertexShader[] =
    "#version 320 es\n"
    "layout (location = 0) in vec4 vertex;\n"
    "out vec2 TexCoords;\n"
    "uniform mat4 projection;\n"
    "void main() {\n"
    "  gl_Position = projection * vec4(vertex.xy, 1.0, 1.0);\n"
    "  TexCoords = vertex.zw;\n"
    "}\n";

const char gFragmentNV12ToRGB[] =
    "#version 320 es\n"
    "precision highp float;\n"
    "in vec2 TexCoords;\n"
    "out vec4 color;\n"
    "uniform sampler2D text;\n"
    "void main() {\n"
    "   float r, g, b, y, u, v;\n"
    "   y = texture(text, TexCoords).r;\n"
    "   u = texture(text, TexCoords).g;\n"
    "   v = texture(text, TexCoords).b;\n"
    "   r = y + 1.13983 * v;\n"
    "   g = y - 0.69801 * v - 0.337633 * u;\n"
    "   b = y + 1.732446 * u;\n"
    "   color = vec4(r, g, b, 1.0);\n"
    "}\n";

/*const char gFragmentNV12ToRGB[] =
    "#version 320 es\n"
    "precision highp float;\n"
    "in vec2 TexCoords;\n"
    "out vec4 color;\n"
    "uniform sampler2D text;\n"
    "vec3 yuv2rgb(in vec3 yuv)\n"
    "{\n"
    "const vec3 offset = vec3(-0.0625, -0.5, -0.5);\n"
    "const vec3 Rcoeff = vec3( 1.164, 0.000,  1.596);\n"
    "const vec3 Gcoeff = vec3( 1.164, -0.391, -0.813);\n"
    "const vec3 Bcoeff = vec3( 1.164, 2.018,  0.000);\n"

    "vec3 rgb;\n"

    "yuv = clamp(yuv, 0.0, 1.0);\n"

    "yuv += offset;\n"

    "rgb.r = dot(yuv, Rcoeff);\n"
    "rgb.g = dot(yuv, Gcoeff);\n"
    "rgb.b = dot(yuv, Bcoeff);\n"
    "return rgb;\n"
    "}\n"

    "vec3 get_yuv_from_texture(in vec2 tcoord)\n"
    "{\n"
    "vec3 yuv;\n"
    "yuv.x = texture(text, TexCoords).r;\n"
    "yuv.y = texture(text, TexCoords).r;\n"
    "yuv.z = texture(text, TexCoords).r;\n"
    "return yuv;\n"
    "}\n"

    "vec4 mytexture2D(in vec2 tcoord)\n"
    "{\n"
    "vec3 rgb, yuv;\n"
    " yuv = get_yuv_from_texture(tcoord);\n"
    "   rgb = yuv2rgb(yuv);\n"
    "   return vec4(rgb, 1.0);\n"
    "}\n"

    "void main()\n"
    "{\n"
    "   color = mytexture2D(TexCoords);\n"
    "}\n";*/

const char gFragmentShader[] =
    "#version 320 es\n"
    "precision mediump float;\n"
    "in vec2 TexCoords;\n"
    "out vec4 color;\n"
    "uniform sampler2D text;\n"
    "void main() {\n"
    "  color = texture(text, TexCoords);\n"
    "}\n";

GLuint
buildShaderProgram(const char *, const char *, const char *);

#endif // SHADER_H
