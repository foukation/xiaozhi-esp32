#pragma once

#include "ai_sdk/types.h"
#include <functional>

namespace ai_sdk {

/**
 * 设备客户端类
 */
class DeviceClient {
public:
    using DeviceInfoSuccessCallback = std::function<void(const DeviceInfoResponse&)>;
    using DeviceInfoErrorCallback = std::function<void(const std::string&)>;

    /**
     * 获取设备信息
     * 设备通过产品信息及设备号从云端获取设备信息，前提是设备号已录入到云端平台
     */
    void obtainDeviceInformation(
        const DeviceInfoRequest& request,
        DeviceInfoSuccessCallback onSuccess,
        DeviceInfoErrorCallback onError
    );

private:
    void onResponse(const std::string& response, DeviceInfoSuccessCallback onSuccess, DeviceInfoErrorCallback onError);
    void onError(const std::string& error, DeviceInfoErrorCallback onError);
};

} // namespace ai_sdk