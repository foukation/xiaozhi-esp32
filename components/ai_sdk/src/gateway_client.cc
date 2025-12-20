#include "ai_sdk/gateway_client.h"
#include "ai_sdk/api_config.h"
#include "ai_sdk/http_client.h"
#include "cJSON.h"
#include "esp_log.h"

namespace ai_sdk {

static const char* TAG = "GatewayClient";

/**
 * 获取网关信息
 *
 * 向云端发起HTTP GET请求获取代理配置。
 *
 * 工作原理：
 * 1. 创建HTTP GET请求到网关接口
 * 2. 设置Content-Type请求头
 * 3. 解析JSON响应，提取代理配置
 * 4. 如果status=1，自动更新全局ApiConfig配置
 *
 * 配置更新：
 * - 如果云端返回status=1（使用代理），自动设置：
 *   - ApiConfig::useAgent = true
 *   - ApiConfig::agentBaseUrl = 代理服务器地址
 *   - ApiConfig::apiToken = 认证令牌
 * - 后续的所有API请求会自动走代理
 *
 * TODO: 需要添加设备认证请求头（X-AI-VID、X-AI-UID等）
 * 参考Android版本的createGateWayHeaders()
 *
 * @param onSuccess 成功回调，返回网关配置
 * @param onError 错误回调
 */
void GatewayClient::getGateWay(GatewayCallback onSuccess, std::function<void(const std::string&)> onError) {
    // 设置请求头
    std::map<std::string, std::string> headers;
    headers["Content-Type"] = "application/json; charset=utf-8";
    // TODO: Add X-AI-VID and X-AI-UID headers when device info is available
    // TODO: 需要添加设备认证信息，参考Android版本的createGateWayHeaders()

    // 发送HTTP GET请求获取网关信息
    HTTPClient http_client;
    http_client.get(
        std::string(ApiConfig::TERMINAL_INTELLIGENT_SERVICE_PLATFORM_BASE_URL) + ApiConfig::GATEWAY_API,
        headers,
        [this, onSuccess](const std::string& response) {
            // 请求成功，解析响应
            onResponse(response, onSuccess, nullptr);
        },
        [this, onError](const std::string& error) {
            // 请求失败，调用错误回调
            onError(error);
        }
    );
}

/**
 * HTTP响应处理回调
 *
 * 解析云端返回的网关配置JSON数据。
 *
 * JSON数据结构：
 * {
 *   "status": 1,          // 是否使用代理（1：使用，0：不使用）
 *   "token": "xxx",       // 认证令牌
 *   "data": {
 *     "http": "http://...", // HTTP代理地址
 *     "ws": "ws://..."      // WebSocket代理地址
 *   },
 *   "expires": 86400      // 有效期（秒）
 * }
 *
 * 解析流程：
 * 1. 解析status字段，判断是否需要使用代理
 * 2. 解析token字段，获取认证令牌
 * 3. 解析data对象中的http和ws代理地址
 * 4. 解析expires字段，获取代理有效期
 * 5. 如果status=1，更新全局ApiConfig配置
 * 6. 调用用户回调
 *
 * 自动配置：如果云端返回status=1，SDK会自动设置全局代理配置，
 * 后续的所有API请求会自动走代理服务器。
 *
 * @param response HTTP响应字符串
 * @param onSuccess 网关信息解析成功后的回调
 * @param onError 网关信息解析失败后的回调
 */
void GatewayClient::onResponse(const std::string& response, GatewayCallback onSuccess, std::function<void(const std::string&)> onError) {
    ESP_LOGI(TAG, "response: %s", response.c_str());

    // 解析JSON响应
    cJSON* root = cJSON_Parse(response.c_str());
    if (!root) {
        // JSON解析失败
        if (onError) {
            onError("Failed to parse JSON response");
        }
        return;
    }

    GatewayInfo gatewayInfo;

    // 解析status字段 - 指示是否使用代理（1：使用，0：不使用）
    cJSON* statusJson = cJSON_GetObjectItem(root, "status");
    if (statusJson && cJSON_IsNumber(statusJson)) {
        gatewayInfo.status = statusJson->valueint;
    }

    // 解析token字段 - 网关认证令牌
    cJSON* tokenJson = cJSON_GetObjectItem(root, "token");
    if (tokenJson && cJSON_IsString(tokenJson)) {
        gatewayInfo.token = tokenJson->valuestring;
    }

    // 解析data对象 - 包含代理服务器地址
    cJSON* dataJson = cJSON_GetObjectItem(root, "data");
    if (dataJson && cJSON_IsObject(dataJson)) {
        // 解析HTTP代理地址
        cJSON* httpJson = cJSON_GetObjectItem(dataJson, "http");
        if (httpJson && cJSON_IsString(httpJson)) {
            gatewayInfo.http_url = httpJson->valuestring;
        }

        // 解析WebSocket代理地址
        cJSON* wsJson = cJSON_GetObjectItem(dataJson, "ws");
        if (wsJson && cJSON_IsString(wsJson)) {
            gatewayInfo.ws_url = wsJson->valuestring;
        }
    }

    // 解析expires字段 - 代理有效期（秒）
    cJSON* expiresJson = cJSON_GetObjectItem(root, "expires");
    if (expiresJson && cJSON_IsNumber(expiresJson)) {
        gatewayInfo.expires_in = expiresJson->valueint;
    }

    // 如果status为1，更新全局ApiConfig配置
    // 这样后续请求会自动使用代理
    if (gatewayInfo.status == 1) {
        ApiConfig::useAgent = true;
        ApiConfig::agentBaseUrl = gatewayInfo.http_url;
        ApiConfig::apiToken = gatewayInfo.token;
    }

    // 释放JSON对象内存
    cJSON_Delete(root);

    // 调用成功回调
    if (onSuccess) {
        onSuccess(gatewayInfo, "");
    }
}

/**
 * 错误处理回调
 *
 * 记录错误日志并调用用户提供的错误回调函数。
 *
 * 错误可能发生在：
 * - HTTP请求失败（网络问题、服务器错误）
 * - JSON解析失败（响应格式错误）
 * - 响应数据不完整（缺少必要字段）
 * - 无法连接到网关服务器
 *
 * 错误处理策略：
 * - 记录详细的错误日志，方便排查问题
 * - 调用用户提供的错误回调，让上层应用决定如何处理
 * - 不自动重试，由上层应用根据业务需求决定是否重试获取
 *
 * @param error 错误信息
 * @param onError 用户提供的错误回调函数
 */
void GatewayClient::onError(const std::string& error, std::function<void(const std::string&)> onError) {
    // 记录错误日志
    ESP_LOGE(TAG, "getGateWay failed: %s", error.c_str());
    // 调用用户提供的错误回调
    if (onError) {
        onError(error);
    }
}

} // namespace ai_sdk