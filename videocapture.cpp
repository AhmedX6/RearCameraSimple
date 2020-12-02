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

static const struct
{
    uint32_t mask;
    const char *str;
} v4l2_buf_flags[] = {
    {V4L2_BUF_FLAG_MAPPED, "MAPPED"},
    {V4L2_BUF_FLAG_QUEUED, "QUEUED"},
    {V4L2_BUF_FLAG_DONE, "DONE"},
    {V4L2_BUF_FLAG_KEYFRAME, "KEYFRAME"},
    {V4L2_BUF_FLAG_PFRAME, "PFRAME"},
    {V4L2_BUF_FLAG_BFRAME, "BFRAME"},
    {V4L2_BUF_FLAG_ERROR, "ERROR"},
    {V4L2_BUF_FLAG_TIMECODE, "TIMECODE"},
    {V4L2_BUF_FLAG_PREPARED, "PREPARED"},
    {V4L2_BUF_FLAG_NO_CACHE_INVALIDATE, "NO_CACHE_INVALIDATE"},
    {V4L2_BUF_FLAG_NO_CACHE_CLEAN, "NO_CACHE_CLEAN"},
    {V4L2_BUF_FLAG_TIMESTAMP_UNKNOWN, "TIMESTAMP_UNKNOWN"},
    {V4L2_BUF_FLAG_TIMESTAMP_MONOTONIC, "TIMESTAMP_MONOTONIC"},
    {V4L2_BUF_FLAG_TIMESTAMP_COPY, "TIMESTAMP_COPY"},
    {V4L2_BUF_FLAG_TSTAMP_SRC_EOF, "TSTAMP_SRC_EOF"},
    {V4L2_BUF_FLAG_TSTAMP_SRC_SOE, "TSTAMP_SRC_SOE"}};

VideoCapture::VideoCapture() : mRunMode(STOPPED), mFrameReady(false)
{
}

bool VideoCapture::open(const char *deviceName)
{
    mDeviceFd = ::open(deviceName, O_RDWR, 0);
    if (mDeviceFd < 0)
    {
        ALOGD("failed to open device %s (%d = %s)", deviceName, errno, strerror(errno));
        return false;
    }

    dstWidth = 720;
    dstHeight = 480;
    dst_coplanar = 1;
    dstFourcc = V4L2_PIX_FMT_NV12;

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

    int ret = subAllocate(V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE, dstWidth,
                          dstHeight, dst_coplanar, dst_colorspace, &dstSize, &dstSize_uv,
                          dstFourcc, dstBuffers, dstBuffers_uv, &dst_numbuf, 1);
    if (ret)
    {
        queueAllBuffers(V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE, dst_numbuf);
        ALOGD("Output Buffers = %d each of size %d\n", dst_numbuf, dstSize);
    }
}

int VideoCapture::subAllocate(int type,
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
                              int interlace)
{
    struct v4l2_format fmt;
    struct v4l2_requestbuffers reqbuf;
    struct v4l2_buffer buffer;
    struct v4l2_plane buf_planes[2];
    int i = 0;
    int ret = -1;

    memset(&fmt, 0, sizeof(fmt));
    fmt.type = type;
    fmt.fmt.pix_mp.width = width;
    fmt.fmt.pix_mp.height = height;
    fmt.fmt.pix_mp.pixelformat = fourcc;
    fmt.fmt.pix_mp.colorspace = clrspc;

    (void)interlace;
    fmt.fmt.pix_mp.field = V4L2_FIELD_ANY;

    /* Set format and update the sizes of y and uv planes
	 * If the returned value is changed, it means driver corrected the size
	 */
    ret = ioctl(mDeviceFd, VIDIOC_S_FMT, &fmt);
    if (ret < 0)
    {
        ALOGD("Cant set color format");
        return 0;
    }
    else
    {
        *sizeimage_y = fmt.fmt.pix_mp.plane_fmt[0].sizeimage;
        *sizeimage_uv = fmt.fmt.pix_mp.plane_fmt[1].sizeimage;
    }

    ALOGD("Current output format: fmt=0x%X, %dx%d, pixels bytes per line=%d",
          fmt.fmt.pix.pixelformat, fmt.fmt.pix.width, fmt.fmt.pix.height, fmt.fmt.pix.bytesperline);

    memset(&reqbuf, 0, sizeof(reqbuf));
    reqbuf.count = *numbuf;
    reqbuf.type = type;
    reqbuf.memory = V4L2_MEMORY_MMAP;

    ret = ioctl(mDeviceFd, VIDIOC_REQBUFS, &reqbuf);
    if (ret < 0)
    {
        ALOGD("Cant request buffers");
        return 0;
    }
    else
    {
        *numbuf = reqbuf.count;
    }

    ALOGD("  %dx%d fmt=%s field=%s cspace=%s flags=%08x",
          fmt.fmt.pix.width, fmt.fmt.pix.height, fourcc_to_string(fmt.fmt.pix.pixelformat), v4l2_field_to_string(&fmt),
          v4l2_colorspace_to_string(&fmt), fmt.fmt.pix.flags);

    for (i = 0; i < *numbuf; i++)
    {

        memset(&buffer, 0, sizeof(buffer));
        buffer.type = type;
        buffer.memory = V4L2_MEMORY_MMAP;
        buffer.index = i;
        buffer.m.planes = buf_planes;
        buffer.length = coplanar ? 2 : 1;

        ret = ioctl(mDeviceFd, VIDIOC_QUERYBUF, &buffer);
        if (ret < 0)
        {
            ALOGD("Cant query buffers");
            return 0;
        }
        ALOGD("query buf, plane 0 = %d, offset 0 = %d", buffer.m.planes[0].length, buffer.m.planes[0].m.mem_offset);

        /* Map all buffers and store the base address of each buffer */
        base[i] = mmap(NULL, buffer.m.planes[0].length, PROT_READ | PROT_WRITE, MAP_SHARED, mDeviceFd, buffer.m.planes[0].m.mem_offset);

        if (MAP_FAILED == base[i])
        {
            while (i >= 0)
            {
                i--;
                munmap(base[i], *sizeimage_y);
                base[i] = NULL;
            }
            ALOGD("Cant mmap buffers Y");
            return 0;
        }

        if (!coplanar)
        {
            continue;
        }

        //ALOGD("query buf, plane 1 = %d, offset 1 = %d", buffer.m.planes[1].length, buffer.m.planes[1].m.mem_offset);
        base_uv[i] = mmap(NULL, buffer.m.planes[1].length, PROT_READ | PROT_WRITE,
                          MAP_SHARED, mDeviceFd, buffer.m.planes[1].m.mem_offset);

        if (MAP_FAILED == base_uv[i])
        {
            while (i >= 0)
            {
                /* Unmap all previous buffers */
                i--;
                munmap(base_uv[i], *sizeimage_uv);
                base[i] = NULL;
            }
            ALOGD("Cant mmap buffers UV");
            return 0;
        }
    }

    if (mRGBPixels != nullptr)
    {
        delete[] mRGBPixels;
        mRGBPixels = nullptr;
    }
    mRGBPixels = new unsigned char[dstWidth * dstHeight * BPP];
    memset(mRGBPixels, 0, dstWidth * dstHeight * BPP);

    mRawCamera = new unsigned char[dstWidth * dstHeight];
    memset(mRGBPixels, 0, dstWidth * dstHeight);
    return 1;
}

int VideoCapture::queueAllBuffers(
    int type,
    int numbuf)
{
    struct v4l2_buffer buffer;
    struct v4l2_plane buf_planes[2];
    int i = 0;
    int ret = -1;
    int lastqueued = -1;

    for (i = 0; i < numbuf; i++)
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

void VideoCapture::streamOn(int type)
{
    int ret = -1;
    ret = ioctl(mDeviceFd, VIDIOC_STREAMON, &type);
    if (-1 == ret)
    {
        ALOGD("Cant Stream on\n");
    }
}

void VideoCapture::streamOFF(int type)
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

    streamOn(V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE);

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

        //off stream v4L2
        if (mRawCamera != nullptr)
        {
            delete[] mRawCamera;
            mRawCamera = nullptr;
        }

        if (mRGBPixels != nullptr)
        {
            delete[] mRGBPixels;
            mRGBPixels = nullptr;
        }
        ALOGD("Capture thread stopped.");
    }
}

void VideoCapture::collectFrames()
{

    //unsigned char *tempBufferARGB = new unsigned char[dstWidth * dstHeight * BPP];
    int field = V4L2_FIELD_TOP;
    while (mRunMode == RUN)
    {
        struct v4l2_buffer buf;
        struct v4l2_plane buf_planes[2];

        dequeue(V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE, &buf, buf_planes);

        ALOGD("%s: dequeued buffer %d (flags:%08x:%s, bytesused:%d, "
              "offset main: %u mplaneOffset: %d buf.length: %d, buf.sequence:%d, buf.m.planes[0].length:%d buf.field %d",
              buf_type_to_string(&buf),
              buf.index, buf.flags, buf_flags_to_string(buf.flags),
              buf.m.planes[0].bytesused, buf.m.offset, buf.m.planes[0].data_offset, buf.length, buf.sequence, buf.m.planes[0].length, buf.field);
        {
            const std::lock_guard<std::mutex> lock(mSafeMutex);
            memcpy(mRawCamera, static_cast<uint8_t *>(dstBuffers[buf.index]), dstWidth * dstHeight);
            /*libyuv::ConvertToARGB(static_cast<uint8_t *>(dstBuffers[buf.index]),
                                  dstWidth * dstHeight, // input size
                                  mRGBPixels,
                                  dstWidth * BPP, // destination stride
                                  0,              // crop_x
                                  0,              // crop_y
                                  dstWidth,       // width
                                  dstHeight,      // height
                                  dstWidth,       // crop width
                                  dstHeight,      // crop height
                                  libyuv::kRotate0, libyuv::FOURCC_NV21);*/
            //libyuv::ARGBToBGRA(tempBufferARGB, dstWidth * BPP, mRGBPixels, dstWidth * BPP, dstWidth, dstHeight);
            //nv21_to_rgb(mRGBPixels, reinterpret_cast<unsigned char *>(dstBuffers[buf.index]), dstWidth, dstHeight);
        }
        queue(V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE, buf.index, field, dstSize, dstSize_uv);
        if (field == V4L2_FIELD_TOP)
            field = V4L2_FIELD_BOTTOM;
        else
            field = V4L2_FIELD_TOP;
    }

    ALOGD("VideoCapture thread ending");
    mRunMode = STOPPED;
}

const char *VideoCapture::buf_type_to_string(struct v4l2_buffer *buf)
{
    switch (buf->type)
    {
    case V4L2_BUF_TYPE_VIDEO_OUTPUT:
    case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
        return "OUTPUT";
    case V4L2_BUF_TYPE_VIDEO_CAPTURE:
    case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
        return "CAPTURE";
    default:
        return "??";
    }
}

const char *VideoCapture::buf_flags_to_string(uint32_t flags)
{
    static char s[256];
    size_t n = 0;

    for (size_t i = 0; i < ARRAY_LENGTH(v4l2_buf_flags); i++)
    {
        if (flags & v4l2_buf_flags[i].mask)
        {
            n += snprintf(s + n, sizeof(s) - n, "%s%s",
                          n > 0 ? "|" : "",
                          v4l2_buf_flags[i].str);
            if (n >= sizeof(s))
                break;
        }
    }

    s[std::min(n, sizeof(s) - 1)] = '\0';

    return s;
}

const char *VideoCapture::v4l2_field_to_string(struct v4l2_format *fmt)
{
    switch (fmt->fmt.pix.field)
    {
        CASE(V4L2_FIELD_ANY)
        CASE(V4L2_FIELD_NONE)
        CASE(V4L2_FIELD_TOP)
        CASE(V4L2_FIELD_BOTTOM)
        CASE(V4L2_FIELD_INTERLACED)
        CASE(V4L2_FIELD_SEQ_TB)
        CASE(V4L2_FIELD_SEQ_BT)
        CASE(V4L2_FIELD_ALTERNATE)
        CASE(V4L2_FIELD_INTERLACED_TB)
        CASE(V4L2_FIELD_INTERLACED_BT)
    default:
        return "unknown";
    }
}

const char *VideoCapture::fourcc_to_string(uint32_t fourcc)
{
    static char s[4];
    uint32_t fmt = htole32(fourcc);

    memcpy(s, &fmt, 4);

    return s;
}

const char *VideoCapture::v4l2_colorspace_to_string(struct v4l2_format *fmt)
{
    switch (fmt->fmt.pix.colorspace)
    {
        CASE(V4L2_COLORSPACE_SMPTE170M)
        CASE(V4L2_COLORSPACE_SMPTE240M)
        CASE(V4L2_COLORSPACE_REC709)
        CASE(V4L2_COLORSPACE_BT878)
        CASE(V4L2_COLORSPACE_470_SYSTEM_M)
        CASE(V4L2_COLORSPACE_470_SYSTEM_BG)
        CASE(V4L2_COLORSPACE_JPEG)
        CASE(V4L2_COLORSPACE_SRGB)
    default:
        return "unknown";
    }
}

/* Queue one buffer (identified by index) */
int VideoCapture::queue(
    int type,
    int index,
    int field,
    int size_y,
    int size_uv)
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
        ALOGD("Failed to queue\n");
        return -1;
    }
    return ret;
}

/*
 * Dequeue one buffer
 * Index of dequeue buffer is returned by ioctl
 */
int VideoCapture::dequeue(int type, struct v4l2_buffer *buf, struct v4l2_plane *buf_planes)
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
        ALOGD("Failed to dequeue\n");
        return -1;
    }
    return ret;
}

unsigned char *VideoCapture::getBufferCamera()
{
    const std::lock_guard<std::mutex> lock(mSafeMutex);
    return mRGBPixels;
}

unsigned char *VideoCapture::getRawBufferCamera()
{
    const std::lock_guard<std::mutex> lock(mSafeMutex);
    return mRawCamera;
}