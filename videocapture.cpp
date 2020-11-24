#include <stdio.h>
#include <stdlib.h>
#include <error.h>
#include <errno.h>
#include <memory.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <cutils/log.h>

#include "assert.h"

#include "videocapture.h"

bool VideoCapture::open(const char *deviceName)
{
    this->mDeviceFd = ::open(deviceName, O_RDWR, 0);
    if (this->mDeviceFd < 0)
    {
        ALOGD("failed to open device %s (%d = %s)", deviceName, errno, strerror(errno));
        return false;
    }

    v4l2_capability caps;
    {
        int result = ioctl(this->mDeviceFd, VIDIOC_QUERYCAP, &caps);
        if (result < 0)
        {
            ALOGD("failed to get device caps for %s (%d = %s)", deviceName, errno, strerror(errno));
            return false;
        }
    }

    ALOGD("Open Device: %s (fd=%d)", deviceName, mDeviceFd);
    ALOGD("  Driver: %s", caps.driver);
    ALOGD("  Card: %s", caps.card);
    ALOGD("  Version: %u.%u.%u", (caps.version >> 16) & 0xFF, (caps.version >> 8) & 0xFF, (caps.version) & 0xFF);
    ALOGD("  All Caps: %08X", caps.capabilities);
    ALOGD("  Dev Caps: %08X", caps.device_caps);

    ALOGD("Supported capture formats:");
    v4l2_fmtdesc formatDescriptions;
    formatDescriptions.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    for (int i = 0; true; i++)
    {
        formatDescriptions.index = i;
        if (ioctl(this->mDeviceFd, VIDIOC_ENUM_FMT, &formatDescriptions) == 0)
        {
            ALOGD("  %2d: %s 0x%08X 0x%X", i, formatDescriptions.description, formatDescriptions.pixelformat, formatDescriptions.flags);
        }
        else
        {
            break;
        }
    }

    if (!(caps.capabilities & V4L2_CAP_VIDEO_CAPTURE) ||
        !(caps.capabilities & V4L2_CAP_STREAMING))
    {
        ALOGD("Streaming capture not supported by %s.", deviceName);
        return false;
    }

    v4l2_format format;
    format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    format.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
    format.fmt.pix.width = 720;
    format.fmt.pix.height = 240;
    format.fmt.pix.field = V4L2_FIELD_ALTERNATE;
    ALOGD("Requesting format %c%c%c%c (0x%08X)",
          ((char *)&format.fmt.pix.pixelformat)[0],
          ((char *)&format.fmt.pix.pixelformat)[1],
          ((char *)&format.fmt.pix.pixelformat)[2],
          ((char *)&format.fmt.pix.pixelformat)[3],
          format.fmt.pix.pixelformat);
    if (ioctl(this->mDeviceFd, VIDIOC_S_FMT, &format) < 0)
    {
        ALOGD("VIDIOC_S_FMT: %s", strerror(errno));
    }

    format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(mDeviceFd, VIDIOC_G_FMT, &format) == 0)
    {

        this->mFormat = format.fmt.pix.pixelformat;
        this->mWidth = format.fmt.pix.width;
        this->mHeight = format.fmt.pix.height;
        this->mStride = format.fmt.pix.bytesperline;

        ALOGD("Current output format: fmt=0x%X, %dx%d, pixels bytes per line=%d", format.fmt.pix.pixelformat, format.fmt.pix.width, format.fmt.pix.height, format.fmt.pix.bytesperline);
    }
    else
    {
        ALOGD("VIDIOC_G_FMT: %s", strerror(errno));
        return false;
    }

    this->mRunMode = STOPPED;
    this->mFrameReady = false;

    return true;
}

void VideoCapture::close()
{
    ALOGD("VideoCapture::close");
    assert(this->mRunMode == STOPPED);

    if (isOpen())
    {
        ALOGD("closing video device file handled %d", this->mDeviceFd);
        ::close(this->mDeviceFd);
        this->mDeviceFd = -1;
    }
}

bool VideoCapture::startStream(std::function<void(VideoCapture *, v4l2_buffer *, unsigned char *)> callback)
{
    ALOGD("Starting stream...");

    int prevRunMode = this->mRunMode.fetch_or(RUN);
    if (prevRunMode & RUN)
    {
        ALOGD("Already in RUN state, so we can't start a new streaming thread");
        return false;
    }

    v4l2_requestbuffers bufrequest;
    bufrequest.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    bufrequest.memory = V4L2_MEMORY_MMAP;
    bufrequest.count = 1;
    if (ioctl(this->mDeviceFd, VIDIOC_REQBUFS, &bufrequest) < 0)
    {
        ALOGD("VIDIOC_REQBUFS: %s", strerror(errno));
        return false;
    }

    memset(&(this->mBufferInfo), 0, sizeof(this->mBufferInfo));
    this->mBufferInfo.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    this->mBufferInfo.memory = V4L2_MEMORY_MMAP;
    this->mBufferInfo.index = 0;
    if (ioctl(this->mDeviceFd, VIDIOC_QUERYBUF, &(this->mBufferInfo)) < 0)
    {
        ALOGD("VIDIOC_QUERYBUF: %s", strerror(errno));
        return false;
    }

    ALOGD("Buffer description => offset: %d & length: %d", this->mBufferInfo.m.offset, this->mBufferInfo.length);

    this->mPixelBuffer = mmap(NULL, this->mBufferInfo.length, PROT_READ | PROT_WRITE, MAP_SHARED, this->mDeviceFd, this->mBufferInfo.m.offset);
    if (mPixelBuffer == MAP_FAILED)
    {
        ALOGD("mmap: %s", strerror(errno));
        return false;
    }
    memset(this->mPixelBuffer, 0, this->mBufferInfo.length);
    ALOGD("Buffer mapped at %p", this->mPixelBuffer);

    if (ioctl(this->mDeviceFd, VIDIOC_QBUF, &(this->mBufferInfo)) < 0)
    {
        ALOGD("VIDIOC_QBUF: %s", strerror(errno));
        return false;
    }

    int type = this->mBufferInfo.type;
    if (ioctl(this->mDeviceFd, VIDIOC_STREAMON, &type) < 0)
    {
        ALOGD("VIDIOC_STREAMON: %s", strerror(errno));
        return false;
    }

    this->mCallback = callback;

    if (this->mRGBPixels != nullptr)
    {
        delete[] this->mRGBPixels;
        this->mRGBPixels = nullptr;
    }
    this->mRGBPixels = new u_int8_t[this->mWidth * this->mHeight * BPP];

    this->mCaptureThread = std::thread([this]() { collectFrames(); });

    ALOGD("Stream started.");
    return true;
}

void VideoCapture::stopStream()
{
    int prevRunMode = this->mRunMode.fetch_or(STOPPING);
    if (prevRunMode == STOPPED)
    {
        this->mRunMode = STOPPED;
    }
    else if (prevRunMode & STOPPING)
    {
        ALOGD("stopStream called while stream is already stopping.  Reentrancy is not supported!");
        return;
    }
    else
    {
        if (this->mCaptureThread.joinable())
        {
            this->mCaptureThread.join();
        }

        int type = this->mBufferInfo.type;
        if (ioctl(this->mDeviceFd, VIDIOC_STREAMOFF, &type) < 0)
        {
            ALOGD("VIDIOC_STREAMOFF: %s", strerror(errno));
        }
        if (this->mRGBPixels != nullptr)
        {
            delete[] this->mRGBPixels;
            this->mRGBPixels = nullptr;
        }
        ALOGD("Capture thread stopped.");
    }

    munmap(this->mPixelBuffer, this->mBufferInfo.length);

    v4l2_requestbuffers bufrequest;
    bufrequest.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    bufrequest.memory = V4L2_MEMORY_MMAP;
    bufrequest.count = 0;
    ioctl(this->mDeviceFd, VIDIOC_REQBUFS, &bufrequest);

    this->mCallback = nullptr;
}

void VideoCapture::markFrameReady()
{
    this->mFrameReady = true;
};

bool VideoCapture::returnFrame()
{
    this->mFrameReady = false;

    if (ioctl(this->mDeviceFd, VIDIOC_QBUF, &(this->mBufferInfo)) < 0)
    {
        ALOGD("VIDIOC_QBUF: %s", strerror(errno));
        return false;
    }

    return true;
}

void VideoCapture::collectFrames()
{
    while (this->mRunMode == RUN)
    {
        if (ioctl(this->mDeviceFd, VIDIOC_DQBUF, &(this->mBufferInfo)) < 0)
        {
            ALOGD("VIDIOC_DQBUF: %s", strerror(errno));
            break;
        }

        Helper::yuv_convert((unsigned char*) this->mPixelBuffer, this->mRGBPixels, this->mWidth, this->mHeight);
        markFrameReady();
        std::this_thread::sleep_for(std::chrono::milliseconds(37));
        markFrameConsumed();
    }

    ALOGD("VideoCapture thread ending");
    this->mRunMode = STOPPED;
}
