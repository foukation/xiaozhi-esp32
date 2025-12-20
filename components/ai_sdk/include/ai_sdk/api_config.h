#pragma once

#include <string>

namespace ai_sdk {

/**
 * API配置管理类
 *
 * 集中管理所有API接口的配置信息，包括：
 * - 云端服务基础URL和接口路径
 * - HTTP请求超时时间
 * - 代理配置（运行时动态调整）
 * - 认证令牌
 *
 * 特点：
 * - 使用静态成员，全局共享配置
 * - 代理配置可在运行时动态修改（通过GatewayClient获取）
 * - 认证信息需要在设备注册后设置
 */
class ApiConfig {
public:
    /**
     * 终端智能服务平台基础URL
     * 所有API请求的基础地址
     */
    static const char* TERMINAL_INTELLIGENT_SERVICE_PLATFORM_BASE_URL;

    /**
     * 网关API路径
     * 获取代理服务器配置
     */
    static const char* GATEWAY_API;

    /**
     * 设备信息获取API路径
     * 设备注册认证接口
     */
    static const char* OBTAIN_DEVICE_INFORMATION_API;

    /**
     * 设备数据上报API路径
     * 心跳/数据上报接口
     */
    static const char* DEVICE_DATA_REPORT_API;

    /**
     * HTTP请求超时时间（毫秒）
     * 默认15秒
     */
    static const long TIMEOUT;

    /**
     * 是否使用代理
     * 由GatewayClient根据云端配置动态设置
     * true: 使用代理服务器，false: 直接连接
     */
    static bool useAgent;

    /**
     * 代理服务器基础URL
     * 当useAgent为true时使用此地址
     */
    static std::string agentBaseUrl;

    /**
     * API访问令牌
     * 用于代理服务器认证
     */
    static std::string apiToken;

    /**
     * 设备认证令牌
     * 设备注册后获得的认证信息
     * TODO: 目前未使用，需要实现设备签名逻辑
     */
    static std::string auth_token;
};

} // namespace ai_sdk