#pragma once

#include "ai_sdk/types.h"
#include <functional>

namespace ai_sdk {

/**
 * 网关客户端类
 * 用于获取网关信息和代理配置
 */
class GatewayClient {
public:
    /**
     * 网关信息获取成功回调类型
     * @param gatewayInfo 网关信息，包含代理地址和认证令牌
     * @param emptyStr 空字符串（用于兼容性）
     */
    using GatewayCallback = std::function<void(const GatewayInfo&, const std::string&)>;

    /**
     * 构造函数
     */
    GatewayClient() = default;

    /**
     * 析构函数
     */
    ~GatewayClient() = default;

    /**
     * 获取网关信息
     *
     * 从云端获取网关配置，包括代理服务器地址和认证令牌。
     *
     * 应用场景：
     * - 设备首次启动时获取代理配置
     * - 代理令牌过期时重新获取
     * - 根据网络环境选择合适的代理服务器
     *
     * 成功后，SDK会自动更新全局ApiConfig配置，后续请求会自动使用代理。
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