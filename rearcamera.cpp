#define LOG_TAG "RearCameraGL"

#include "rearcamera.h"

RearCamera::RearCamera()
{
    mSession = new android::SurfaceComposerClient();
    mProgram = 0;
    mColorShaderHandle = -1;
    mProjectionShaderHandle = -1;
    VBO = 0;
    VAO = 0;
    idTextureGearNeutral = 0;
    idTextureGearReverse = 0;
    idTextureGearForward = 0;
    idTextureHandBreak = 0;
    cameraTexId = 0;
    mVideoCapture.open("/dev/video14");
}

void RearCamera::checkGlError(const char *op)
{
    for (GLint error = glGetError(); error; error = glGetError())
    {
        ALOGD("after %s() glError (0x%x)\n", op, error);
    }
}

bool RearCamera::initShadersProgram()
{
    mProgram = buildShaderProgram(gVertexShader, gFragmentNV12ToRGB, "RearCamera");
    if (!mProgram)
    {
        ALOGD("Could not create program.");
        return false;
    }
    return true;
}

bool RearCamera::initSurface()
{
    ALOGD("initSurface() started");

    android::sp<android::IBinder> dtoken(android::SurfaceComposerClient::getBuiltInDisplay(android::ISurfaceComposer::eDisplayIdMain));
    android::DisplayInfo dinfo;
    android::status_t status = android::SurfaceComposerClient::getDisplayInfo(dtoken, &dinfo);
    if (status)
    {
        ALOGD("initSurface() getDisplayInfo status not OK");
        return false;
    }
    ALOGD("Global surface size is w=%d h=%d", dinfo.w, dinfo.h);
    android::sp<android::SurfaceControl> control = mSession->createSurface(android::String8("RearCamera"), dinfo.w, dinfo.h, android::PIXEL_FORMAT_RGB_888);
    android::SurfaceComposerClient::Transaction{}
        .setLayer(control, 0x7FFFFFFF)
        .setSize(control, dinfo.w, dinfo.h)
        .apply();

    android::sp<android::Surface> s = control->getSurface();

    // initialize opengl and egl
    const EGLint attribs[] = {
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_DEPTH_SIZE, 0,
        EGL_NONE};
    EGLint w, h;
    EGLint numConfigs;
    EGLConfig config;
    EGLSurface surface;
    EGLContext context;

    EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);

    eglInitialize(display, NULL, NULL);
    eglChooseConfig(display, attribs, &config, 1, &numConfigs);
    if ((surface = eglCreateWindowSurface(display, config, s.get(), NULL)) == EGL_NO_SURFACE)
    {
        ALOGD("initSurface() eglCreateWindowSurface failed");
        return false;
    }
    const EGLint context_attribs[] = {EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};
    context = eglCreateContext(display, config, NULL, context_attribs);
    eglQuerySurface(display, surface, EGL_WIDTH, &w);
    eglQuerySurface(display, surface, EGL_HEIGHT, &h);
    mSurfaceWidth = w;
    mSurfaceHeight = h;
    ALOGD("Surface size is w = %d h = %d", w, h);

    if (eglMakeCurrent(display, surface, surface, context) == EGL_FALSE)
    {
        ALOGD("initSurface() eglMakeCurrent failed");
        return false;
    }

    mDisplay = display;
    mContext = context;
    mSurface = surface;
    mFlingerSurfaceControl = control;
    mFlingerSurface = s;

    ALOGD("initSurface() done successfully");
    return true;
}

bool RearCamera::initSurfaceConfigs()
{
    if (initShadersProgram())
    {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthFunc(GL_ALWAYS);
        glDisable(GL_CULL_FACE);

        mColorShaderHandle = glGetUniformLocation(mProgram, "textColor");
        ALOGD("glGetUniformLocation(\"textColor\") = %d\n", mColorShaderHandle);

        mProjectionShaderHandle = glGetUniformLocation(mProgram, "projection");
        ALOGD("glGetUniformLocation(\"projection\") = %d\n", mProjectionShaderHandle);

        mTextShaderHandle = glGetUniformLocation(mProgram, "text");
        ALOGD("glGetUniformLocation(\"text\") = %d\n", mTextShaderHandle);

        mTextUVShaderHandle = glGetUniformLocation(mProgram, "textUV");
        ALOGD("glGetUniformLocation(\"glGetUniformLocation\") = %d\n", mTextUVShaderHandle);

        glViewport(GL_ZERO, GL_ZERO, mSurfaceWidth, mSurfaceHeight);

        glUseProgram(mProgram);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);

        glBindVertexArray(VAO);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * 6 * 4, GL_ZERO, GL_DYNAMIC_DRAW);
        glEnableVertexAttribArray(GL_ZERO);
        glVertexAttribPointer(GL_ZERO, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), GL_ZERO);
        glBindBuffer(GL_ARRAY_BUFFER, GL_ZERO);
        glBindVertexArray(GL_ZERO);

        glm::mat4 projection = glm::ortho(0.0f, static_cast<GLfloat>(mSurfaceWidth), 0.0f, static_cast<GLfloat>(mSurfaceHeight));
        glUniformMatrix4fv(mProjectionShaderHandle, 1, GL_FALSE, glm::value_ptr(projection));

        ALOGD("OpenGL paramaters set correctly");
        return true;
    }

    return false;
}

void RearCamera::initAllTexturesFromPng()
{
    std::vector<std::string> images = Helper::listDirectory("", ".png");
    if (images.size() > 0)
    {
        for (unsigned int i = 0; i < images.size(); i++)
        {
            loadPngFromPath(Helper::basename(images.at(i)), images.at(i));
        }
    }
}

void RearCamera::createCameraTexture(const int width, const int height)
{
    /*if (cameraTexId == 0)
    {
        GLuint idTex;
        glGenTextures(1, &idTex);
        glBindTexture(GL_TEXTURE_2D, idTex);

        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        glTexImage2D(GL_TEXTURE_2D, GL_ZERO, GL_RED, width, height, GL_ZERO, GL_RED, GL_UNSIGNED_BYTE, NULL);
        glBindTexture(GL_TEXTURE_2D, GL_ZERO);

        cameraTexId = idTex;
        ALOGD("Camera Texture id is: %d h:%d height:%d", cameraTexId, width, height);
    }*/

    if (cameraTexY == 0)
    {
        GLuint idTex;
        glGenTextures(1, &idTex);
        glBindTexture(GL_TEXTURE_2D, idTex);

        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        glTexImage2D(GL_TEXTURE_2D, GL_ZERO, GL_RED, width, height, GL_ZERO, GL_RED, GL_UNSIGNED_BYTE, NULL);
        glBindTexture(GL_TEXTURE_2D, GL_ZERO);

        cameraTexU = idTex;
    }

    if (cameraTexU == 0)
    {
        GLuint idTex;
        glGenTextures(1, &idTex);
        glBindTexture(GL_TEXTURE_2D, idTex);

        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        glTexImage2D(GL_TEXTURE_2D, GL_ZERO, GL_RED, width, height, GL_ZERO, GL_RED, GL_UNSIGNED_BYTE, NULL);
        glBindTexture(GL_TEXTURE_2D, GL_ZERO);

        cameraTexU = idTex;
    }

    if (cameraTexV == 0)
    {
        GLuint idTex;
        glGenTextures(1, &idTex);
        glBindTexture(GL_TEXTURE_2D, idTex);

        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        glTexImage2D(GL_TEXTURE_2D, GL_ZERO, GL_RED, width, height, GL_ZERO, GL_RED, GL_UNSIGNED_BYTE, NULL);
        glBindTexture(GL_TEXTURE_2D, GL_ZERO);

        cameraTexV = idTex;
    }
}

void RearCamera::refreshCamera()
{
    GLfloat xpos = 0;
    GLfloat ypos = 0;

    GLfloat w = mSurfaceWidth;
    GLfloat h = mSurfaceHeight;

    GLfloat vertices[6][4] = {
        {xpos, ypos + h, 0.0, 0.0},
        {xpos, ypos, 0.0, 1.0},
        {xpos + w, ypos, 1.0, 1.0},

        {xpos, ypos + h, 0.0, 0.0},
        {xpos + w, ypos, 1.0, 1.0},
        {xpos + w, ypos + h, 1.0, 0.0}};

    /*if (cameraTexId != 0)
    {
        glBindTexture(GL_TEXTURE_2D, cameraTexId);
        glUniform1i(mTextShaderHandle, GL_ZERO);
        glBindVertexArray(VAO);

        glActiveTexture(GL_TEXTURE0);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glTexSubImage2D(GL_TEXTURE_2D, GL_ZERO, GL_ZERO, GL_ZERO, mVideoCapture.getWidth(),
                        mVideoCapture.getHeight(), GL_RED, GL_UNSIGNED_BYTE, std::get<0>(mVideoCapture.getRawBufferCamera()));
        glBufferSubData(GL_ARRAY_BUFFER, GL_ZERO, sizeof(vertices), vertices);

        glDrawArrays(GL_TRIANGLES, GL_ZERO, 6);

        //end
        glBindTexture(GL_TEXTURE_2D, GL_ZERO);
        glBindBuffer(GL_ARRAY_BUFFER, GL_ZERO);
    }*/
}

void RearCamera::initEverything()
{
    //this order is important
    if (initSurface())
    {
        if (initSurfaceConfigs())
        {
            //initAllTexturesFromPng();
            createCameraTexture(mVideoCapture.getWidth(), mVideoCapture.getHeight());
            startCapture();
        }
    }
}

void RearCamera::startCapture()
{
    mVideoCapture.startStream();
}

void RearCamera::printAll()
{
    glClearColor(GL_ZERO, GL_ZERO, GL_ZERO, GL_ZERO);
    glClear(GL_COLOR_BUFFER_BIT);
    refreshCamera();

    eglSwapBuffers(mDisplay, mSurface);
}

bool RearCamera::loadPngFromPath(const std::string &textureName, const std::string &fileName)
{
    png_structp png_ptr;
    png_infop info_ptr;
    unsigned int sig_read = 0;
    int color_type, interlace_type;
    FILE *fp;

    if ((fp = fopen(fileName.c_str(), "rb")) == NULL)
        return false;

    png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);

    if (png_ptr == NULL)
    {
        fclose(fp);
        return false;
    }

    info_ptr = png_create_info_struct(png_ptr);
    if (info_ptr == NULL)
    {
        fclose(fp);
        png_destroy_read_struct(&png_ptr, NULL, NULL);
        return false;
    }

    if (setjmp(png_jmpbuf(png_ptr)))
    {
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        fclose(fp);
        return false;
    }

    png_init_io(png_ptr, fp);
    png_set_sig_bytes(png_ptr, sig_read);

    png_read_png(png_ptr, info_ptr, PNG_TRANSFORM_STRIP_16 | PNG_TRANSFORM_PACKING | PNG_TRANSFORM_EXPAND, NULL);

    png_uint_32 width, height;
    int bit_depth;
    png_get_IHDR(png_ptr, info_ptr, &width, &height, &bit_depth, &color_type, &interlace_type, NULL, NULL);

    unsigned int row_bytes = png_get_rowbytes(png_ptr, info_ptr);
    GLubyte *outData = (unsigned char *)malloc(row_bytes * height);

    png_bytepp row_pointers = png_get_rows(png_ptr, info_ptr);

    for (unsigned int i = 0; i < height; i++)
    {
        memcpy(outData + (row_bytes * i), row_pointers[i], row_bytes);
    }

    png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
    fclose(fp);

    GLuint idTex;
    glGenTextures(1, &idTex);
    glBindTexture(GL_TEXTURE_2D, idTex);

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, GL_ZERO, GL_RGBA, width, height, GL_ZERO, GL_RGBA, GL_UNSIGNED_BYTE, outData);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    free(outData);

    Texture texture = {
        textureName,
        fileName,
        idTex,
        glm::ivec2(width, height)};
    mTextures.insert(std::pair<std::string, Texture>(textureName, texture));

    return true;
}

void RearCamera::clearAll()
{
    mVideoCapture.stopStream();
    mVideoCapture.close();
    eglMakeCurrent(mDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    eglDestroyContext(mDisplay, mContext);
    eglDestroySurface(mDisplay, mSurface);
    mFlingerSurface.clear();
    mFlingerSurfaceControl.clear();
    eglTerminate(mDisplay);
    eglReleaseThread();
    mSession.clear();
    ALOGD("RearCamera cleaned");
}

RearCamera::~RearCamera()
{
    clearAll();
}
