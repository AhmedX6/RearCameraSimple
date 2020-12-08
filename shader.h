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
    "layout(binding = 0) uniform sampler2D textureY;\n"
    "layout(binding = 1) uniform sampler2D textureUV;\n"
    "void main() {\n"
    "   float r, g, b, y, u, v;\n"
    "   y = texture(textureY, TexCoords).r;\n"
    "   u = texture(textureUV, TexCoords).r - 0.5;\n"
    "   v = texture(textureUV, TexCoords).a - 0.5;\n"
    "   r = y + 1.13983 * v;\n"
    "   g = y - 0.39465 * u - 0.58060 * v;\n"
    "   b = y + 2.03211 * u;\n"
    "   color = vec4(r, g, b, 1.0);\n"
    "}\n";

const char gFragmentShader[] =
    "#version 320 es\n"
    "precision mediump float;\n"
    "in vec2 TexCoords;\n"
    "out vec4 color;\n"
    "uniform sampler2D text;\n"
    "void main() {\n"
    "  color = texture(text, TexCoords);\n"
    "}\n";

GLuint buildShaderProgram(const char *, const char *, const char *);

#endif // SHADER_H
