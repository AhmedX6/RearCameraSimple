#ifndef VIDEO_CAPTURE_H_
#define VIDEO_CAPTURE_H_

#include <atomic>
#include <thread>
#include <functional>
#include <linux/videodev2.h>
#include "helper.h"

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "RearCamera"

const int BPP = 3;

class VideoCapture
{
public:
  bool open(const char *deviceName);
  void close();

  bool startStream(std::function<void(VideoCapture *, v4l2_buffer *, unsigned char *)> callback = nullptr);
  void stopStream();

  // Valid only after open()
  __u32 getWidth() { return mWidth; };
  __u32 getHeight() { return mHeight; };
  __u32 getStride() { return mStride; };
  __u32 getV4LFormat() { return mFormat; };

  unsigned char *getLatestData() { return mRGBPixels; };

  bool isFrameReady() { return mFrameReady; };
  void markFrameConsumed() { returnFrame(); };

  bool isOpen() { return mDeviceFd >= 0; };

private:
  void collectFrames();
  void markFrameReady();
  bool returnFrame();

  int mDeviceFd = -1;

  v4l2_buffer mBufferInfo = {};
  void *mPixelBuffer = nullptr;

  __u32 mFormat = 0;
  __u32 mWidth = 0;
  __u32 mHeight = 0;
  __u32 mStride = 0;

  std::function<void(VideoCapture *, v4l2_buffer *, unsigned char *)> mCallback;

  std::thread mCaptureThread;
  std::atomic<int> mRunMode;
  std::atomic<bool> mFrameReady;
  unsigned char *mRGBPixels = nullptr;

  enum RunModes
  {
    STOPPED = 0,
    RUN = 1,
    STOPPING = 2,
  };
};

#endif //VIDEO_CAPTURE_H_
