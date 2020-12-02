#ifndef REARCAMERA_H_
#define REARCAMERA_H_

#include <stdint.h>
#include <inttypes.h>
#include <sys/inotify.h>
#include <sys/poll.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <math.h>
#include <fcntl.h>
#include <utils/misc.h>
#include <signal.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>

#include <cutils/atomic.h>
#include <cutils/properties.h>

#include <androidfw/AssetManager.h>
#include <binder/IPCThreadState.h>
#include <utils/Errors.h>
#include <utils/SystemClock.h>

#include <android-base/properties.h>

#include <ui/PixelFormat.h>
#include <ui/Rect.h>
#include <ui/Region.h>
#include <ui/DisplayInfo.h>

#include <gui/ISurfaceComposer.h>
#include <gui/Surface.h>
#include <gui/SurfaceComposerClient.h>
#include <algorithm>

#include <glm.hpp>
#include <gtc/matrix_transform.hpp>
#include <gtc/type_ptr.hpp>

#include <png.h>

#include "helper.h"
#include "shader.h"
#include "videocapture.h"

namespace android
{
	class Surface;
	class SurfaceComposerClient;
	class SurfaceControl;
} // namespace android

class RearCamera
{
public:
	RearCamera();
	~RearCamera();

	void initEverything();
	void startCapture();
	void printAll();

private:
	//full setup
	bool initShadersProgram();
	bool initSurface();
	bool initSurfaceConfigs();
	void initAllTexturesFromPng();
	void checkGlError(const char *);
	bool loadPngFromPath(const std::string &, const std::string &);
	void forwardFrame(v4l2_buffer *, unsigned char *);
	void createCameraTexture(const int, const int);

	void printTexture(const std::string &, GLfloat, GLfloat, glm::ivec2, glm::vec3);
	void refreshCamera();
	void clearAll();

	struct Texture
	{
		std::string name;
		std::string path;
		GLuint TextureID;
		glm::ivec2 Size;
	};

private:
	android::sp<android::SurfaceComposerClient> mSession;
	android::sp<android::SurfaceControl> mFlingerSurfaceControl;
	android::sp<android::Surface> mFlingerSurface;

	EGLDisplay mDisplay;
	EGLDisplay mContext;
	EGLDisplay mSharedContext;
	EGLDisplay mSurface;

	int mSurfaceWidth;
	int mSurfaceHeight;
	std::map<std::string, Texture> mTextures;

	VideoCapture mVideoCapture;

	GLuint mProgram;
	GLint mColorShaderHandle;
	GLint mProjectionShaderHandle;
	GLint mTextShaderHandle;

	GLuint cameraTexId;

	GLuint cameraTexY;
	GLuint cameraTexU;
	GLuint cameraTexV;

	GLuint VBO;
	GLuint VAO;

	GLuint idTextureGearNeutral;
	GLuint idTextureGearReverse;
	GLuint idTextureGearForward;
	GLuint idTextureHandBreak;
};

#endif // REARCAMERA_H_
