#define LOG_TAG "RearCamera"

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

VideoCapture::VideoCapture() : mRunMode(STOPPED), mFrameReady(false),
                               mCameraWidth(CAMERA_WIDTH), mCameraHeight(CAMERA_HEIGHT), mCameraFourCC(CAMERA_FOURCC), mNbrBuffers(6)
{
}

VideoCapture::~VideoCapture()
{
    if (mRawCamera != nullptr)
    {
        delete[] mRawCamera;
        mRawCamera = nullptr;
    }

    if (mRawColorCamera != nullptr)
    {
        delete[] mRawColorCamera;
        mRawColorCamera = nullptr;
    }
}

bool VideoCapture::open(const char *deviceName)
{
    mDeviceFd = ::open(deviceName, O_RDWR, 0);
    if (mDeviceFd < 0)
    {
        ALOGD("failed to open device %s (%d = %s)", deviceName, errno, strerror(errno));
        return false;
    }

    allocateBuffers(deviceName);
    return true;
}

void VideoCapture::allocateBuffers(const char *deviceName)
{
    v4l2_capability caps;
    {
        int result = ioctl(mDeviceFd, VIDIOC_QUERYCAP, &caps);
        if (result < 0)
        {
            ALOGD("failed to get device caps for %s (%d = %s)", deviceName, errno, strerror(errno));
            return;
        }
    }

    ALOGD("Open Device: %s (fd=%d)", deviceName, mDeviceFd);
    ALOGD("  Driver: %s", caps.driver);
    ALOGD("  Card: %s", caps.card);
    ALOGD("  Version: %u.%u.%u", (caps.version >> 16) & 0xFF, (caps.version >> 8) & 0xFF, (caps.version) & 0xFF);
    ALOGD("  All Caps: %08X", caps.capabilities);
    ALOGD("  Dev Caps: %08X", caps.device_caps);

    int ret = prepare();
    if (ret)
    {
        queueAllBuffers(CAMERA_CAPTURE_MODE);
    }
}

int VideoCapture::prepare()
{
    struct v4l2_format fmt;
    struct v4l2_requestbuffers reqbuf;
    struct v4l2_buffer buffer;
    struct v4l2_plane buf_planes[2];
    int ret = -1;

    memset(&fmt, 0, sizeof(fmt));
    fmt.type = CAMERA_CAPTURE_MODE;
    fmt.fmt.pix_mp.width = mCameraWidth;
    fmt.fmt.pix_mp.height = mCameraHeight;
    fmt.fmt.pix_mp.pixelformat = mCameraFourCC;
    fmt.fmt.pix_mp.colorspace = V4L2_COLORSPACE_SMPTE170M;
    fmt.fmt.pix_mp.field = V4L2_FIELD_ANY;

    ret = ioctl(mDeviceFd, VIDIOC_S_FMT, &fmt);
    if (ret < 0)
    {
        ALOGD("Cant set format");
        return 0;
    }

    mYBufferSize = fmt.fmt.pix_mp.plane_fmt[0].sizeimage;
    mUVBufferSize = fmt.fmt.pix_mp.plane_fmt[1].sizeimage;

    ALOGD("Current output format: fmt=0x%X, %dx%d, pixels bytes per line=%d",
          fmt.fmt.pix.pixelformat, fmt.fmt.pix.width, fmt.fmt.pix.height, fmt.fmt.pix.bytesperline);

    memset(&reqbuf, 0, sizeof(reqbuf));
    reqbuf.count = mNbrBuffers;
    reqbuf.type = CAMERA_CAPTURE_MODE;
    reqbuf.memory = V4L2_MEMORY_MMAP;

    ret = ioctl(mDeviceFd, VIDIOC_REQBUFS, &reqbuf);
    if (ret < 0)
    {
        ALOGD("Cant request buffers");
        return 0;
    }
    else
    {
        mNbrBuffers = reqbuf.count;
    }

    ALOGD("  %dx%d flags=%08x numbuffers:%d", fmt.fmt.pix.width, fmt.fmt.pix.height, fmt.fmt.pix.flags, mNbrBuffers);

    for (int i = 0; i < mNbrBuffers; i++)
    {

        memset(&buffer, 0, sizeof(buffer));
        buffer.type = CAMERA_CAPTURE_MODE;
        buffer.memory = V4L2_MEMORY_MMAP;
        buffer.index = i;
        buffer.m.planes = buf_planes;
        buffer.length = 2;

        ret = ioctl(mDeviceFd, VIDIOC_QUERYBUF, &buffer);
        if (ret < 0)
        {
            ALOGD("Cant query buffers");
            return 0;
        }
        ALOGD("query buf, plane 0 = %d, offset 0 = %d", buffer.m.planes[0].length, buffer.m.planes[0].m.mem_offset);

        mPointerBuffersY[i] = mmap(NULL, buffer.m.planes[0].length, PROT_READ | PROT_WRITE, MAP_SHARED,
                                   mDeviceFd, buffer.m.planes[0].m.mem_offset);

        if (MAP_FAILED == mPointerBuffersY[i])
        {
            while (i >= 0)
            {
                i--;
                munmap(mPointerBuffersY[i], mYBufferSize);
                mPointerBuffersY[i] = NULL;
            }
            ALOGD("Cant mmap buffers Y");
            return 0;
        }

        ALOGD("query buf, plane 1 = %d, offset 1 = %d, sizeimage_uv %d", buffer.m.planes[1].length, buffer.m.planes[1].m.mem_offset, mYBufferSize);
        mPointerBuffersUV[i] = mmap(NULL, buffer.m.planes[1].length, PROT_READ | PROT_WRITE,
                                    MAP_SHARED, mDeviceFd, buffer.m.planes[1].m.mem_offset);

        if (MAP_FAILED == mPointerBuffersUV[i])
        {
            while (i >= 0)
            {
                i--;
                munmap(mPointerBuffersUV[i], mUVBufferSize);
                mPointerBuffersY[i] = NULL;
            }
            ALOGD("Cant mmap buffers UV");
            return 0;
        }
    }

    if (mRawColorCamera != nullptr)
    {
        delete[] mRawColorCamera;
        mRawColorCamera = nullptr;
    }
    if (mRawCamera != nullptr)
    {
        delete[] mRawCamera;
        mRawCamera = nullptr;
    }

    mRawCamera = new unsigned char[mCameraWidth * mCameraHeight];
    mRawColorCamera = new unsigned char[mCameraWidth * mCameraHeight / 2];

    memset(mRawCamera, 0, mCameraWidth * mCameraHeight);
    memset(mRawColorCamera, 0, mCameraWidth * mCameraHeight / 2);

    ALOGD("Output Buffers = %d each of size %d\n", mNbrBuffers, mYBufferSize);

    return 1;
}

int VideoCapture::queueAllBuffers(int type)
{
    struct v4l2_buffer buffer;
    struct v4l2_plane buf_planes[2];
    int i = 0;
    int ret = -1;
    int lastqueued = -1;

    for (i = 0; i < mNbrBuffers; i++)
    {
        memset(&buffer, 0, sizeof(buffer));
        buffer.type = type;
        buffer.memory = V4L2_MEMORY_MMAP;
        buffer.index = i;
        buffer.m.planes = buf_planes;
        buffer.length = 2;
        gettimeofday(&buffer.timestamp, NULL);

        ret = ioctl(mDeviceFd, VIDIOC_QBUF, &buffer);
        if (-1 == ret)
        {
            break;
        }
        else
        {
            lastqueued = i;
        }
    }
    return lastqueued;
}

void VideoCapture::startV4Lstream(int type)
{
    int ret = -1;
    ret = ioctl(mDeviceFd, VIDIOC_STREAMON, &type);
    if (-1 == ret)
    {
        ALOGD("Cant Stream on\n");
    }
}

void VideoCapture::stopV4Lstream(int type)
{
    int ret = -1;
    ret = ioctl(mDeviceFd, VIDIOC_STREAMOFF, &type);
    if (-1 == ret)
    {
        ALOGD("Cant Stream on\n");
    }
}

void VideoCapture::close()
{
    ALOGD("VideoCapture::close");
    assert(mRunMode == STOPPED);

    if (isOpen())
    {
        ALOGD("closing video device file handled %d", mDeviceFd);
        ::close(mDeviceFd);
        mDeviceFd = -1;
    }
}

bool VideoCapture::startStream()
{
    ALOGD("Starting stream...");

    int prevRunMode = mRunMode.fetch_or(RUN);
    if (prevRunMode & RUN)
    {
        ALOGD("Already in RUN state, so we can't start a new streaming thread");
        return false;
    }

    startV4Lstream(CAMERA_CAPTURE_MODE);

    mCaptureThread = std::thread([this]() { collectFrames(); });

    ALOGD("Stream started.");
    return true;
}

void VideoCapture::stopStream()
{
    int prevRunMode = mRunMode.fetch_or(STOPPING);
    if (prevRunMode == STOPPED)
    {
        mRunMode = STOPPED;
    }
    else if (prevRunMode & STOPPING)
    {
        ALOGD("stopStream called while stream is already stopping. Reentrancy is not supported!");
        return;
    }
    else
    {
        if (mCaptureThread.joinable())
        {
            mCaptureThread.join();
        }

        ALOGD("Capture thread stopped.");
    }
}

void VideoCapture::collectFrames()
{
    struct v4l2_buffer buf;
    struct v4l2_plane buf_planes[2];
    while (mRunMode == RUN)
    {
        dequeueFrame(CAMERA_CAPTURE_MODE, &buf, buf_planes);
        ALOGD("dequeued buffer %d (flags:%08x, bytesused:%d, "
              "offset main: %u mplaneOffset: %d buf.length: %d, buf.sequence:%d, buf.m.planes[0].length:%d buf.field %d buf.m.planes[buf.index].bytesused:%d",
              buf.index, buf.flags, buf.m.planes[0].bytesused, buf.m.offset, buf.m.planes[0].data_offset, buf.length,
              buf.sequence, buf.m.planes[0].length, buf.field, buf.m.planes[buf.index].bytesused);
        {
            const std::lock_guard<std::mutex> lock(mSafeMutex);
            memcpy(mRawCamera, mPointerBuffersY[buf.index], mCameraWidth * mCameraHeight);
            memcpy(mRawColorCamera, mPointerBuffersUV[buf.index], mCameraWidth * mCameraHeight / 2);
        }
        queueFrame(CAMERA_CAPTURE_MODE, buf.index, V4L2_FIELD_NONE, mYBufferSize, mUVBufferSize);
    }

    ALOGD("VideoCapture thread ending");
    mRunMode = STOPPED;
}

int VideoCapture::queueFrame(int type, int index, int field, int size_y, int size_uv)
{
    struct v4l2_buffer buffer;
    struct v4l2_plane buf_planes[2];
    int ret = -1;

    buf_planes[0].length = buf_planes[0].bytesused = size_y;
    buf_planes[1].length = buf_planes[1].bytesused = size_uv;
    buf_planes[0].data_offset = buf_planes[1].data_offset = 0;

    memset(&buffer, 0, sizeof(buffer));
    buffer.type = type;
    buffer.memory = V4L2_MEMORY_MMAP;
    buffer.index = index;
    buffer.m.planes = buf_planes;
    buffer.field = field;
    buffer.length = 2;
    gettimeofday(&buffer.timestamp, NULL);

    ret = ioctl(mDeviceFd, VIDIOC_QBUF, &buffer);
    if (-1 == ret)
    {
        ALOGD("Failed to queueFrame\n");
        return -1;
    }
    return ret;
}

int VideoCapture::dequeueFrame(int type, struct v4l2_buffer *buf, struct v4l2_plane *buf_planes)
{
    int ret = -1;

    memset(buf, 0, sizeof(*buf));
    buf->type = type;
    buf->memory = V4L2_MEMORY_MMAP;
    buf->m.planes = buf_planes;
    buf->length = 2;
    ret = ioctl(mDeviceFd, VIDIOC_DQBUF, buf);
    if (-1 == ret)
    {
        ALOGD("Failed to dequeueFrame\n");
        return -1;
    }
    return ret;
}

std::tuple<const unsigned char *, const unsigned char *> VideoCapture::getRawBufferCamera()
{
    const std::lock_guard<std::mutex> lock(mSafeMutex);
    return std::make_tuple(mRawCamera, mRawColorCamera);
}