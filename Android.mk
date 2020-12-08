rearcam_CommonCFlags = -DGL_GLEXT_PROTOTYPES -DEGL_EGLEXT_PROTOTYPES
rearcam_CommonCFlags += -Wall -Werror -Wunused -Wunreachable-code -fexceptions -pthread

LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_CFLAGS += ${rearcam_CommonCFlags}

LOCAL_SHARED_LIBRARIES := \
    libandroidfw \
    libhidlbase \
    libhidltransport \
    libbase \
    libui \
    libhardware \
    libbinder \
    libcutils \
    liblog \
    libutils \
    libEGL \
    libGLESv3 \
    libgui \
    libjpeg \
    libpng \
    android.hardware.automotive.vehicle@2.0

LOCAL_C_INCLUDES += \
    external/libpng \
    external/vulkan-validation-layers/libs/glm \

LOCAL_SRC_FILES += \
    rearcamera_main.cpp \
    shader.cpp \
    rearcamera.cpp \
    helper.cpp \
    videocapture.cpp \

LOCAL_STATIC_LIBRARIES += cpufeatures

LOCAL_MODULE:= rearcam
LOCAL_PRELINK_MODULE := false
LOCAL_INIT_RC := rearcamera.rc

ifdef TARGET_32_BIT_SURFACEFLINGER
	LOCAL_32_BIT_ONLY := true
endif

include $(BUILD_EXECUTABLE)
