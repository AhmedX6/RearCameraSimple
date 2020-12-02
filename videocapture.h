#ifndef VIDEO_CAPTURE_H_
#define VIDEO_CAPTURE_H_

#include <atomic>
#include <thread>
#include <functional>
#include <linux/videodev2.h>
#include <condition_variable>
#include <endian.h>
#include <mutex>
#include "helper.h"
#include "yuv2rgb.h"

const int BPP = 4;

#define CASE(ENUM) \
  case ENUM:       \
    return #ENUM;
#define ARRAY_LENGTH(x) (sizeof(x) / sizeof(*(x)))
class VideoCapture
{
public:
  VideoCapture();

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
  int getWidth() { return dstWidth; };
  int getHeight() { return dstHeight; };

  unsigned char *getBufferCamera();
  unsigned char *getRawBufferCamera();

  bool isOpen() { return mDeviceFd >= 0; };

private:
  void collectFrames();

  std::mutex mSafeMutex;

  int mDeviceFd = -1;

  std::thread mCaptureThread;
  std::atomic<int> mRunMode;
  std::atomic<bool> mFrameReady;
  unsigned char *mRGBPixels = nullptr;
  unsigned char *mRawCamera = nullptr;

  //new
  int dstHeight = 0;
  int dstWidth = 0;
  int dstSize = 0;
  int dstSize_uv = 0;
  int dstFourcc = 0;
  int dst_coplanar = 0;
  enum v4l2_colorspace dst_colorspace = V4L2_COLORSPACE_SMPTE170M;

  void *dstBuffers[6];
  void *dstBuffers_uv[6];
  int dst_numbuf = 6;

  //new methods
  const char *buf_type_to_string(struct v4l2_buffer *buf);
  const char *buf_flags_to_string(uint32_t flags);
  const char *v4l2_field_to_string(struct v4l2_format *fmt);
  const char *v4l2_colorspace_to_string(struct v4l2_format *fmt);
  const char *fourcc_to_string(uint32_t fourcc);

  void allocateBuffers(const char *name);
  int subAllocate(
      int type,
      int width,
      int height,
      int coplanar,
      enum v4l2_colorspace clrspc,
      int *sizeimage_y,
      int *sizeimage_uv,
      int fourcc,
      void *base[],
      void *base_uv[],
      int *numbuf,
      int interlace);

  int queueAllBuffers(int type, int numbuf);
  void streamOFF(int type);
  void streamOn(int type);

  int dequeue(int type, struct v4l2_buffer *buf, struct v4l2_plane *buf_planes);
  int queue(int type, int index, int field, int size_y, int size_uv);
};

#endif //VIDEO_CAPTURE_H_
