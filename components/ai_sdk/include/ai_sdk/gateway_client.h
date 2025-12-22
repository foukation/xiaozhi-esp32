#pragma once

#include "ai_sdk/types.h"
#include <functional>

namespace ai_sdk {

/**
 * @brief 网关客户端类
 * 用于获取网关信息和代理配置。
 * 与Android端API保持一致。
 */
class GatewayClient {
public:
    /**
     * @brief 网关信息获取成功回调类型
     * @param gatewayInfo 网关信息，包含代理地址和认证令牌
     * @param emptyStr 空字符串（用于兼容性）
     */
    using GatewayCallback = std::function<void(const GatewayInfo&, const std::string&)>;

    /**
     * @brief 构造函数
     */
    GatewayClient() = default;

    /**
     * @brief 析构函数
     */
    ~GatewayClient() = default;

    /**
     * @brief 获取网关信息
     *
     * 从云端获取网关配置，包括代理服务器地址和认证令牌。
     * 与Android端API保持一致。
     *
     * 应用场景：
     * - 设备首次启动时获取代理配置
     * - 代理令牌过期时重新获取
     * - 根据网络环境选择合适的代理服务器
     *
     * 响应结构（与Android端一致）：
     * {
     *   "status": 1,              // 是否使用代理（1：使用，0：不使用）
     *   "token": "xxx",           // 认证令牌（可为空）
     *   "data": {
     *     "http": "http://...",   // HTTP代理地址
     *     "ws": "ws://..."        // WebSocket代理地址
     *   },
     *   "expires": 86400          // 有效期（秒）
     * }
     *
     * 成功后，SDK会自动更新全局ApiConfig配置，后续请求会自动使用代理。
     * 如果status=AgentUseCode.USE(1)，则启用代理；否则禁用代理。
     *
     * 使用示例：
     * @code
     * GatewayClient client;
     * client.getGateWay(
     *     [](const GatewayInfo& gatewayInfo, const std::string&) {
     *         // 处理成功响应
     *         if (gatewayInfo.status == AgentUseCode::USE) {
     *             ESP_LOGI("app", "Gateway enabled:");
     *             ESP_LOGI("app", "  HTTP: %s", gatewayInfo.data.http.c_str());
     *             ESP_LOGI("app", "  WS: %s", gatewayInfo.data.ws.c_str());
     *         } else {
     *             ESP_LOGI("app", "Gateway disabled");
     *         }
     *     },
     *     [](const std::string& error) {
     *         // 处理错误
     *         ESP_LOGE("app", "Get gateway failed: %s", error.c_str());
     *     });
     * @endcode
     *
     * @param onSuccess 成功回调，返回网关信息（代理地址、令牌、有效期）
     * @param onError 错误回调，返回错误信息
     */
    void getGateWay(GatewayCallback onSuccess, std::function<void(const std::string&)> onError);

private:
    /**
     * HTTP响应处理回调
     * 解析云端返回的网关配置JSON数据
     *
     * @param response HTTP响应字符串
     * @param onSuccess 网关信息解析成功后的回调
     * @param onError 网关信息解析失败后的回调
     */
    void onResponse(const std::string& response, GatewayCallback onSuccess, std::function<void(const std::string&)> onError);

    /**
     * 错误处理回调
     * 处理网关信息获取过程中的错误
     *
     * @param error 错误信息
     * @param onError 用户提供的错误回调函数
     */
    void onError(const std::string& error, std::function<void(const std::string&)> onError);
};

} // namespace ai_sdk