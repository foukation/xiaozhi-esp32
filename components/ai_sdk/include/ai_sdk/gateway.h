#pragma once

#include "ai_sdk/gateway_client.h"
#include "ai_sdk/device_client.h"
#include "ai_sdk/report_client.h"

namespace ai_sdk {

/**
 * 网关管理类
 *
 * 网关管理类是AI SDK的入口点，采用单例模式设计。
 * 它聚合了三个核心客户端，提供统一的访问接口。
 *
 * 设计模式：单例模式（线程安全，C++11后static local initialization保证）
 * - 确保整个应用程序中只有一个Gateway实例
 * - 全局访问点：Gateway::getInstance()
 *
 * 聚合的子系统：
 * - GatewayClient: 网关/代理配置管理
 * - DeviceClient: 设备注册和认证
 * - ReportClient: 数据上报和心跳管理
 *
 * 使用示例：
 * Gateway* gateway = Gateway::getInstance();
 * gateway->getDeviceClient()->obtainDeviceInformation(...);
 */
class Gateway {
public:
    /**
     * 构造函数
     * 初始化各个客户端子系统
     */
    Gateway();

    /**
     * 析构函数
     * 自动清理各个客户端资源
     */
    ~Gateway();

    /**
     * 获取网关单例实例
     * @return 网关实例指针，每次调用返回同一实例
     */
    static Gateway* getInstance();

    /**
     * 获取网关客户端
     * @return 网关客户端指针，用于代理配置管理
     */
    GatewayClient* getGatewayClient() { return &gateway_client_; }

    /**
     * 获取设备客户端
     * @return 设备客户端指针，用于设备注册认证
     */
    DeviceClient* getDeviceClient() { return &device_client_; }

    /**
     * 获取上报客户端
     * @return 上报客户端指针，用于数据上报和心跳
     */
    ReportClient* getReportClient() { return &report_client_; }

private:
    /**
     * 网关单例实例
     * 程序启动时初始化，程序结束时自动销毁
     */
    static Gateway instance_;

    /**
     * 网关客户端实例
     * 负责网关信息获取和代理配置
     */
    GatewayClient gateway_client_;

    /**
     * 设备客户端实例
     * 负责设备注册认证
     */
    DeviceClient device_client_;

    /**
     * 上报客户端实例
     * 负责设备数据上报和心跳管理
     */
    ReportClient report_client_;
};

} // namespace ai_sdk