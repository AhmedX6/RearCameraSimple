#define LOG_TAG "RearCameraMain"

#include <stdint.h>
#include <inttypes.h>

#include <binder/IPCThreadState.h>
#include <binder/ProcessState.h>
#include <binder/IServiceManager.h>
#include <cutils/properties.h>
#include <sys/resource.h>
#include <utils/Log.h>
#include <utils/SystemClock.h>
#include <utils/threads.h>
#include <android-base/properties.h>
#include "signal.h"

#include "rearcamera.h"

static volatile bool exitFromAppFlag = false;

static void sigHandler(int sig)
{
    if (sig == SIGINT || sig == SIGTERM)
    {
        exitFromAppFlag = true;
    }
}

void waitForSurfaceFlinger()
{
    int64_t waitStartTime = android::elapsedRealtime();
    android::sp<android::IServiceManager> sm = android::defaultServiceManager();
    const android::String16 name("SurfaceFlinger");
    const int SERVICE_WAIT_SLEEP_MS = 100;
    const int LOG_PER_RETRIES = 10;
    int retry = 0;
    while (sm->checkService(name) == nullptr)
    {
        retry++;
        if ((retry % LOG_PER_RETRIES) == 0)
        {
            ALOGD("Waiting for SurfaceFlinger, waited for %" PRId64 " ms", android::elapsedRealtime() - waitStartTime);
        }
        usleep(SERVICE_WAIT_SLEEP_MS * 1000);
    };
    int64_t totalWaited = android::elapsedRealtime() - waitStartTime;
    if (totalWaited > SERVICE_WAIT_SLEEP_MS)
    {
        ALOGD("Waiting for SurfaceFlinger took %" PRId64 " ms", totalWaited);
    }
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    setpriority(PRIO_PROCESS, 0, ANDROID_PRIORITY_DISPLAY);

    waitForSurfaceFlinger();

    ALOGD("RearCamera set up, let's rock !");

    struct sigaction sigIntHandler;
    memset(&sigIntHandler, 0, sizeof(struct sigaction));
    sigIntHandler.sa_handler = sigHandler;
    sigemptyset(&sigIntHandler.sa_mask);
    sigIntHandler.sa_flags = 0;

    sigaction(SIGINT, &sigIntHandler, NULL);
    sigaction(SIGTERM, &sigIntHandler, NULL);

    RearCamera rearCamera;
    if (rearCamera.initEverything())
    {
        while (!exitFromAppFlag)
        {
            rearCamera.printAll();
        }
    }
    else
    {
        ALOGD("Fail starting rear camera...");
    }

    ALOGD("RearCamera exit");
    return 0;
}
