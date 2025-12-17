#pragma once

#include "ai_sdk/gateway_client.h"
#include "ai_sdk/device_client.h"
#include "ai_sdk/report_client.h"

namespace ai_sdk {

/**
 * 网关管理类
 * 用于管理和处理与网关相关的操作和数据
 */
class Gateway {
public:
    Gateway();
    ~Gateway();

    /**
     * 获取网关实例，每次调用返回同一实例
     */
    static Gateway* getInstance();

    /**
     * 获取网关客户端
     */
    GatewayClient* getGatewayClient() { return &gateway_client_; }

    /**
     * 获取设备客户端
     */
    DeviceClient* getDeviceClient() { return &device_client_; }

    /**
     * 获取上报客户端
     */
    ReportClient* getReportClient() { return &report_client_; }

private:
    static Gateway instance_;
    GatewayClient gateway_client_;
    DeviceClient device_client_;
    ReportClient report_client_;
};

} // namespace ai_sdk