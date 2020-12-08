#ifndef VIDEO_CAPTURE_H_
#define VIDEO_CAPTURE_H_

#include <atomic>
#include <thread>
#include <functional>
#include <linux/videodev2.h>
#include <condition_variable>
#include <tuple>
#include <endian.h>
#include <mutex>
#include "helper.h"

static constexpr int CAMERA_WIDTH = 720;
static constexpr int CAMERA_HEIGHT = 480;
static constexpr int CAMERA_FOURCC = V4L2_PIX_FMT_NV21M;
static constexpr int CAMERA_CAPTURE_MODE = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;

class VideoCapture
{
public:
  VideoCapture();
  ~VideoCapture();

  enum RunModes
  {
    STOPPED = 0,
    RUN = 1,
    STOPPING = 2,
  };

  bool open(const char *deviceName);
  void close();

  bool startStream();
  void stopStream();

  // Valid only after open()
  int getWidth() { return mCameraWidth; };
  int getHeight() { return mCameraHeight; };

  std::tuple<const unsigned char *, const unsigned char *> getRawBufferCamera();

  bool isOpen() { return mDeviceFd >= 0; };

private:
  void collectFrames();

  std::mutex mSafeMutex;

  int mDeviceFd = -1;

  std::thread mCaptureThread;
  std::atomic<int> mRunMode;
  std::atomic<bool> mFrameReady;

  unsigned char *mRawCamera = nullptr;
  unsigned char *mRawColorCamera = nullptr;

  int mCameraWidth = 0;
  int mCameraHeight = 0;
  int mCameraFourCC = 0;

  int mNbrBuffers = 6;

  int mYBufferSize = 0;
  int mUVBufferSize = 0;

  void *mPointerBuffersY[6];
  void *mPointerBuffersUV[6];

  void allocateBuffers(const char *name);
  int prepare();

  int queueAllBuffers(int type);
  void stopV4Lstream(int type);
  void startV4Lstream(int type);

  int dequeueFrame(int type, struct v4l2_buffer *buf, struct v4l2_plane *buf_planes);
  int queueFrame(int type, int index, int field, int size_y, int size_uv);
};

#endif //VIDEO_CAPTURE_H_
