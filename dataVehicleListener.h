#ifndef DATAVEHICLELISTENER_H
#define DATAVEHICLELISTENER_H

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <unistd.h>
#include <utils/Log.h>
#include <errno.h>

#include <android/hardware/automotive/vehicle/2.0/IVehicle.h>
#include <android/hardware/automotive/vehicle/2.0/types.h>
#include <android/hardware/automotive/vehicle/2.0/IVehicleCallback.h>

using namespace android;
using namespace android::hardware;
using namespace android::hardware::automotive::vehicle::V2_0;

constexpr int GEAR_SELECTION = static_cast<int>(VehicleProperty::GEAR_SELECTION);

class DataVehicleListener : public IVehicleCallback
{
public:
    Return<void> onPropertyEvent(const hidl_vec<VehiclePropValue> &values) override
    {
        ALOGD("onPropertyEvent");
        for (auto it = values.begin(); it != values.end(); it++)
        {
            ALOGD("Prop:0x%x", it->prop);
            if (it->prop == GEAR_SELECTION)
            {
                if (mCallbackGearData != nullptr)
                {
                    if (it->value.int32Values[0] == static_cast<int>(VehicleGear::GEAR_REVERSE))
                    {
                        mCallbackGearData(true);
                    }
                    else
                    {
                        mCallbackGearData(false);
                    }
                }
            }
        }
        return Return<void>();
    }

    Return<void> onPropertySet(const VehiclePropValue & /*value*/) override
    {
        ALOGD("onPropertySet");
        return Return<void>();
    }

    Return<void> onPropertySetError(StatusCode /* errorCode */,
                                    int32_t /* propId */,
                                    int32_t /* areaId */) override
    {
        ALOGD("onPropertySetError");
        return Return<void>();
    }

    void setCallback(std::function<void(bool)> callback = nullptr) { mCallbackGearData = callback; }

private:
    std::function<void(bool)> mCallbackGearData;
};

#endif //DATAVEHICLELISTENER_H
