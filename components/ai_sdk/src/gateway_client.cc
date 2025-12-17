#include "ai_sdk/gateway_client.h"
#include "ai_sdk/api_config.h"
#include "ai_sdk/http_client.h"
#include "cJSON.h"
#include "esp_log.h"

namespace ai_sdk {

static const char* TAG = "GatewayClient";

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

void GatewayClient::onError(const std::string& error, std::function<void(const std::string&)> onError) {
    // 记录错误日志
    ESP_LOGE(TAG, "getGateWay failed: %s", error.c_str());
    // 调用用户提供的错误回调
    if (onError) {
        onError(error);
    }
}

} // namespace ai_sdk