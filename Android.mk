rearcam_CommonCFlags = -DGL_GLEXT_PROTOTYPES -DEGL_EGLEXT_PROTOTYPES
rearcam_CommonCFlags += -Wall -Werror -Wunused -Wunreachable-code -fexceptions -pthread

# rearcam executable
# =========================================================

LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_CFLAGS += ${rearcam_CommonCFlags}

LOCAL_SHARED_LIBRARIES := \
    libandroidfw \
    libbase \
    libui \
    libhardware \
    libbinder \
    libcutils \
    liblog \
    libutils \
    libEGL \
    libGLESv2 \
    libgui \
    libjpeg \
    libpng

LOCAL_C_INCLUDES += \
    external/libpng \
    external/vulkan-validation-layers/libs/glm \
    external/libyuv/files/include

LOCAL_SRC_FILES += \
    rearcamera_main.cpp \
    shader.cpp \
    rearcamera.cpp \
    helper.cpp \
    videocapture.cpp \
    yuv2rgb.cpp

LOCAL_STATIC_LIBRARIES += cpufeatures libyuv_static

LOCAL_MODULE:= rearcam

ifdef TARGET_32_BIT_SURFACEFLINGER
	LOCAL_32_BIT_ONLY := true
endif

include $(BUILD_EXECUTABLE)
