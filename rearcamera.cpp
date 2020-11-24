#include "rearcamera.h"

RearCamera::RearCamera()
{
    this->mSession = new android::SurfaceComposerClient();
    this->mProgram = 0;
    this->mTargetPixels = nullptr;
    this->mColorShaderHandle = -1;
    this->mProjectionShaderHandle = -1;
    this->mTagShaderHandle = -1;
    this->mIgnoreColor = -1;
    this->VBO = 0;
    this->VAO = 0;
    this->idTextureGearNeutral = 0;
    this->idTextureGearReverse = 0;
    this->idTextureGearForward = 0;
    this->idTextureHandBreak = 0;
    this->cameraTexId = 0;
    this->mVideoCapture.open("/dev/video14");
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
    this->mProgram = buildShaderProgram(gVertexShader, gFragmentShader, "RearCamera");
    if (!this->mProgram)
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
    this->mSurfaceWidth = w;
    this->mSurfaceHeight = h;
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

        this->mColorShaderHandle = glGetUniformLocation(this->mProgram, "textColor");
        ALOGD("glGetUniformLocation(\"textColor\") = %d\n", this->mColorShaderHandle);

        this->mProjectionShaderHandle = glGetUniformLocation(this->mProgram, "projection");
        ALOGD("glGetUniformLocation(\"projection\") = %d\n", this->mProjectionShaderHandle);

        this->mTextShaderHandle = glGetUniformLocation(this->mProgram, "text");
        ALOGD("glGetUniformLocation(\"text\") = %d\n", this->mTextShaderHandle);

        this->mTagShaderHandle = glGetUniformLocation(this->mProgram, "tag");
        ALOGD("glGetUniformLocation(\"tag\") = %d\n", this->mTagShaderHandle);

        this->mIgnoreColor = glGetUniformLocation(this->mProgram, "ignoreColor");
        ALOGD("glGetUniformLocation(\"ignoreColor\") = %d\n", this->mIgnoreColor);

        glViewport(GL_ZERO, GL_ZERO, this->mSurfaceWidth, this->mSurfaceHeight);

        glUseProgram(this->mProgram);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

        glGenVertexArrays(1, &this->VAO);
        glGenBuffers(1, &this->VBO);

        glBindVertexArray(this->VAO);
        glBindBuffer(GL_ARRAY_BUFFER, this->VBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * 6 * 4, GL_ZERO, GL_DYNAMIC_DRAW);
        glEnableVertexAttribArray(GL_ZERO);
        glVertexAttribPointer(GL_ZERO, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), GL_ZERO);
        glBindBuffer(GL_ARRAY_BUFFER, GL_ZERO);
        glBindVertexArray(GL_ZERO);

        glm::mat4 projection = glm::ortho(0.0f, static_cast<GLfloat>(this->mSurfaceWidth), 0.0f, static_cast<GLfloat>(this->mSurfaceHeight));
        glUniformMatrix4fv(this->mProjectionShaderHandle, 1, GL_FALSE, glm::value_ptr(projection));

        ALOGD("OpenGL paramaters set correctly");
        return true;
    }

    return false;
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
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    free(outData);

    Texture texture = {
        textureName,
        fileName,
        idTex,
        glm::ivec2(width, height)};
    this->mTextures.insert(std::pair<std::string, Texture>(textureName, texture));

    return true;
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
    if (this->cameraTexId == 0)
    {
        GLuint idTex;
        glGenTextures(1, &idTex);
        glBindTexture(GL_TEXTURE_2D, idTex);

        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D, GL_ZERO, GL_RGB, width, height, GL_ZERO, GL_RGB, GL_UNSIGNED_BYTE, NULL);

        glBindTexture(GL_TEXTURE_2D, GL_ZERO);

        if (this->mTargetPixels != nullptr)
        {
            delete[] this->mTargetPixels;
            this->mTargetPixels = nullptr;
        }
        this->mTargetPixels = new uint8_t[width * height * BPP];
        this->cameraTexId = idTex;
        ALOGD("Camera Texture id is: %d", this->cameraTexId);
    }
}

void RearCamera::initEverything()
{
    //this order is important
    if (initSurface())
    {
        if (initSurfaceConfigs())
        {
            initAllTexturesFromPng();
            createCameraTexture(this->mVideoCapture.getWidth(), this->mVideoCapture.getHeight());
            startCapture();
        }
    }
}

void RearCamera::startCapture()
{
    this->mVideoCapture.startStream();
}

void RearCamera::refreshCamera()
{
    glBindTexture(GL_TEXTURE_2D, this->cameraTexId);
    glUniform1i(this->mIgnoreColor, 1);
    glUniform1i(this->mTextShaderHandle, 0);
    glUniform1i(this->mTagShaderHandle, 1);
    glBindVertexArray(this->VAO);

    GLfloat xpos = 0;
    GLfloat ypos = 0;

    GLfloat w = this->mSurfaceWidth;
    GLfloat h = this->mSurfaceHeight;

    GLfloat vertices[6][4] = {
        {xpos, ypos + h, 0.0, 0.0},
        {xpos, ypos, 0.0, 1.0},
        {xpos + w, ypos, 1.0, 1.0},

        {xpos, ypos + h, 0.0, 0.0},
        {xpos + w, ypos, 1.0, 1.0},
        {xpos + w, ypos + h, 1.0, 0.0}};

    glActiveTexture(GL_TEXTURE0);
    glBindBuffer(GL_ARRAY_BUFFER, this->VBO);
    glTexSubImage2D(GL_TEXTURE_2D, GL_ZERO, GL_ZERO, GL_ZERO, this->mVideoCapture.getWidth(), this->mVideoCapture.getHeight(), GL_RGB, GL_UNSIGNED_BYTE, this->mVideoCapture.getLatestData());
    glBufferSubData(GL_ARRAY_BUFFER, GL_ZERO, sizeof(vertices), vertices);
    glDrawArrays(GL_TRIANGLES, GL_ZERO, 6);
    glBindBuffer(GL_ARRAY_BUFFER, GL_ZERO);
    glBindTexture(GL_TEXTURE_2D, GL_ZERO);
}

void RearCamera::printAll()
{
    glClearColor(GL_ZERO, GL_ZERO, GL_ZERO, GL_ZERO);
    glClear(GL_COLOR_BUFFER_BIT);
    refreshCamera();

    eglSwapBuffers(this->mDisplay, this->mSurface);
}

void RearCamera::clearAll()
{
    this->mVideoCapture.stopStream();
    this->mVideoCapture.close();
    eglMakeCurrent(this->mDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    eglDestroyContext(this->mDisplay, this->mContext);
    eglDestroySurface(this->mDisplay, this->mSurface);
    this->mFlingerSurface.clear();
    this->mFlingerSurfaceControl.clear();
    eglTerminate(this->mDisplay);
    eglReleaseThread();
    this->mSession.clear();
    if (this->mTargetPixels != nullptr)
        delete[] this->mTargetPixels;
    ALOGD("RearCamera cleaned");
}

RearCamera::~RearCamera()
{
    clearAll();
}
