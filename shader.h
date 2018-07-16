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

const char gVertexShader[] =
    "#version 320 es\n"
    "layout (location = 0) in vec4 vertex;\n"
    "out vec2 TexCoords;\n"
    "uniform mat4 projection;\n"
    "void main() {\n"
    "  gl_Position = projection * vec4(vertex.xy, 1.0, 1.0);\n"
    "  TexCoords = vertex.zw;\n"
    "}\n";

const char gFragmentShader[] =
    "#version 320 es\n"
    "precision mediump float;\n"
    "in vec2 TexCoords;\n"
    "out vec4 color;\n"
    "uniform int tag;\n"
    "uniform int ignoreColor;\n"
    "uniform sampler2D text;\n"
    "uniform vec3 textColor;\n"
    "void main() {\n"
    "  float alpha;\n"
    "  if (tag > 0)\n"
    "     alpha = texture(text, TexCoords).a;\n"
    "  else\n"
    "     alpha = texture(text, TexCoords).r;\n"
    "  if (ignoreColor == 0)\n"
    "     color = vec4(textColor, alpha);\n"
    "  else\n"
    "     color = texture(text, TexCoords);\n"
    "}\n";


const char gFragmentYUVShader[] =
    "#version 320 es\n"
    "precision mediump float;\n"
    "uniform sampler2D texY;\n"
    "uniform sampler2D texU;\n"
    "uniform sampler2D texV;\n"
    "in vec2 TexCoords;\n"
    
    "vec3 yuv2rgb(in vec3 yuv) {\n"
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

    "vec3 get_yuv_from_texture(in vec2 tcoord) {\n"
    "vec3 yuv;\n"
    "yuv.x = texture(texY, tcoord).r;\n"
    "yuv.y = texture(texU, tcoord).r;\n"    
    "yuv.z = texture(texV, tcoord).r;\n"
    "return yuv;\n"
    "}\n"

    "vec4 mytexture2D(in vec2 tcoord) {\n"
    "vec3 rgb, yuv;\n"
    "yuv = get_yuv_from_texture(tcoord);\n"
    "rgb = yuv2rgb(yuv);\n"
    "return vec4(rgb, 1.0);\n"
    "}\n"

    "out vec4 out_color;\n"

    "void main() {\n"
    "out_color = mytexture2D(TexCoords);\n"
    "}\n";

const char gFragmentYUV2Shader[] =
    "#version 320 es\n"
    "precision mediump float;\n"
    "out vec4 FragColor;\n"
    "uniform int FrameDirection;\n"
    "uniform int FrameCount;\n"
    "uniform vec2 OutputSize;\n"
    "uniform vec2 TextureSize;\n"
    "uniform vec2 InputSize;\n"
    "uniform sampler2D Texture;\n"
    "uniform sampler2D Pass3Texture;\n"
    "in vec2 TexCoords;\n"

    "#define SourceSize vec4(TextureSize, 1.0 / TextureSize)\n"
    "#define OutSize vec4(OutputSize, 1.0 / OutputSize)\n"

    "void main() {\n"
	"vec4 inputY = texture(Pass3Texture, TexCoords);\n"
	"vec4 inputUV = texture(Texture, TexCoords);\n"

	"vec4 yuva = vec4(inputY.x, (inputUV.y - 0.5), (inputUV.z - 0.5), 1.0);\n"

	"vec4 rgba = vec4(0.0);\n"

	"rgba.r = yuva.x * 1.0 + yuva.y * 0.0 + yuva.z * 1.4;\n"
	"rgba.g = yuva.x * 1.0 + yuva.y * -0.343 + yuva.z * -0.711;\n"
	"rgba.b = yuva.x * 1.0 + yuva.y * 1.765 + yuva.z * 0.0;\n"
	"rgba.a = 1.0;\n"
	
	"FragColor = rgba;\n"
    "}\n";

const char gFragmentYUV3Shader[] =
    "#version 320 es\n"
    "precision mediump float;\n"
    "uniform sampler2D textureSampler;\n"
    "uniform sampler2D paritySampler;\n"
    "out vec4 FragColor;\n"
    "out vec2 lineCounter;\n"
    "out vec2 yPlane;\n"
    "out vec2 uPlane;\n"
    "out vec2 vPlane;\n"
    "void main() {\n"
    "vec2 offset_even = vec2(texture(paritySampler, lineCounter).x * 0.5, 0.0);\n"
    "vec2 offset_odd = vec2(0.5 - offset_even.x, 0.0);\n"
    "float yChannel = texture(textureSampler, yPlane).x;\n"
    "float uChannel = texture(textureSampler, uPlane + offset_even).x;\n"
    "float vChannel = texture(textureSampler, vPlane + offset_odd).x;\n"
    "vec4 channels = vec4(yChannel, uChannel, vChannel, 1.0);\n"
    "mat4 conversion = mat4( 1.0, 1.0, 1.0, 0.0, 0.0, -0.344, 1.772, 0.0, 1.402, -0.714, 0.0, 0.0, -0.701, 0.529, -0.886, 1.0);\n"
    "FragColor = conversion * channels;"
    "}\n";

GLuint buildShaderProgram(const char *, const char *, const char *);

#endif // SHADER_H
