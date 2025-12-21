#include "ai_sdk/device_client.h"
#include "ai_sdk/api_config.h"
#include "ai_sdk/http_client.h"
#include "cJSON.h"
#include "esp_log.h"
#include <cstdlib>

namespace ai_sdk {

static const char* TAG = "DeviceClient";

/**
 * 获取设备信息
 *
 * 通过产品信息和设备号向云端发起设备注册认证请求。
 *
 * 流程：
 * 1. 构建JSON请求体，包含设备类型、设备号、产品ID和密钥
 * 2. 发送HTTP POST请求到设备认证接口
 * 3. 解析JSON响应，提取deviceId和deviceSecret
 *
 * @param request 设备信息请求结构
 * @param onSuccess 成功回调，返回设备信息
 * @param onError 错误回调，返回错误信息
 */
void DeviceClient::obtainDeviceInformation(
    const DeviceInfoRequest& request,
    DeviceInfoSuccessCallback onSuccess,
    DeviceInfoErrorCallback onError
) {
    // 输出API调用日志（与Android RequestApi保持一致）
    ESP_LOGI(TAG, "API: obtainDeviceInformation");

    // 构建JSON请求体
    cJSON* root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "deviceNoType", request.deviceNoType.c_str());  // 设备号类型
    cJSON_AddStringToObject(root, "deviceNo", request.deviceNo.c_str());          // 设备编号
    cJSON_AddStringToObject(root, "productId", request.productId.c_str());        // 产品ID
    cJSON_AddStringToObject(root, "productKey", request.productKey.c_str());      // 产品密钥

    // 转换为无格式化的JSON字符串
    char* jsonStr = cJSON_PrintUnformatted(root);
    std::string jsonBody(jsonStr);
    free(jsonStr);  // 释放cJSON分配的内存
    cJSON_Delete(root);  // 释放JSON对象

    // 创建请求头
    std::map<std::string, std::string> headers;
    headers["Content-Type"] = "application/json; charset=utf-8";
    // TODO: Add sign, deviceNo, productId, productKey, ts, deviceId headers
    // 需要设备密钥来生成签名，参考Android版本的createAIServerHeaders()

    // 输出请求参数日志（与Android OkHttpManager保持一致）
    ESP_LOGI(TAG, "request params: %s", jsonBody.c_str());

    // 发送HTTP POST请求获取设备信息
    HTTPClient http_client;
    http_client.post(
        std::string(ApiConfig::TERMINAL_INTELLIGENT_SERVICE_PLATFORM_BASE_URL) + ApiConfig::OBTAIN_DEVICE_INFORMATION_API,
        jsonBody,
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
 * 处理HTTP响应回调
 *
 * 解析云端返回的JSON数据，提取设备认证信息。
 *
 * 解析流程：
 * 1. 解析JSON响应字符串
 * 2. 提取status字段（状态码）
 * 3. 提取data对象中的deviceId和deviceSecret
 * 4. 释放JSON内存
 * 5. 调用用户提供的回调函数
 *
 * @param response HTTP响应字符串
 * @param onSuccess 设备信息解析成功后的回调
 * @param onError 设备信息解析失败后的回调
 */
void DeviceClient::onResponse(const std::string& response, DeviceInfoSuccessCallback onSuccess, DeviceInfoErrorCallback onError) {
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

    DeviceInfoResponse deviceInfoResponse;

    // 解析status字段 - 请求状态码
    cJSON* statusJson = cJSON_GetObjectItem(root, "status");
    if (statusJson && cJSON_IsNumber(statusJson)) {
        deviceInfoResponse.status = statusJson->valueint;
    }

    // 解析data对象 - 包含设备认证信息
    cJSON* dataJson = cJSON_GetObjectItem(root, "data");
    if (dataJson && cJSON_IsObject(dataJson)) {
        // 解析deviceId - 设备唯一标识
        cJSON* deviceIdJson = cJSON_GetObjectItem(dataJson, "deviceId");
        if (deviceIdJson && cJSON_IsString(deviceIdJson)) {
            deviceInfoResponse.data.deviceId = deviceIdJson->valuestring;
        }

        // 解析deviceSecret - 设备密钥（用于后续请求签名）
        cJSON* deviceSecretJson = cJSON_GetObjectItem(dataJson, "deviceSecret");
        if (deviceSecretJson && cJSON_IsString(deviceSecretJson)) {
            deviceInfoResponse.data.deviceSecret = deviceSecretJson->valuestring;
        }
    }

    // 释放JSON对象内存
    cJSON_Delete(root);

    // 调用成功回调
    if (onSuccess) {
        onSuccess(deviceInfoResponse);
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
 *
 * @param error 错误信息
 * @param onError 用户提供的错误回调函数
 */
void DeviceClient::onError(const std::string& error, DeviceInfoErrorCallback onError) {
    // 记录错误日志
    ESP_LOGE(TAG, "obtainDeviceInformation failed: %s", error.c_str());
    // 调用用户提供的错误回调
    if (onError) {
        onError(error);
    }
}

} // namespace ai_sdk