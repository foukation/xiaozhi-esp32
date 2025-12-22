#include "ai_sdk/device_client.h"
#include "ai_sdk/api_config.h"
#include "ai_sdk/http_client.h"
#include "ai_sdk/ai_assistant_manager.h"
#include "cJSON.h"
#include "esp_log.h"
#include <cstdlib>

namespace ai_sdk {

static const char* TAG = "DeviceClient";

/**
 * @brief 获取设备信息
 *
 * 通过产品信息和设备号向云端发起设备注册认证请求。
 * 与Android端API保持一致。
 *
 * 请求结构（与Android端一致）：
 * {
 *   "deviceNoType": "MAC|SN|IMEI",  // 设备号类型
 *   "deviceNo": "设备唯一序列号",      // 设备号，产品内唯一
 *   "productId": "产品ID",           // 平台创建产品时生成
 *   "productKey": "产品密钥"         // 平台创建产品时生成
 * }
 *
 * 响应结构（与Android端一致）：
 * {
 *   "status": 200,                   // 成功=200，其他值为异常
 *   "message": "响应消息",           // 错误提示信息
 *   "data": {
 *     "deviceId": "设备ID",         // 平台上唯一设备标识
 *     "deviceNo": "设备号",         // 产品内唯一标识设备的序列号
 *     "productId": "产品ID",        // 产品ID，平台创建产品时生成
 *     "deviceSecret": "设备密钥"    // 设备密钥，平台创建产品时生成
 *   }
 * }
 *
 * 流程：
 * 1. 构建JSON请求体，包含设备类型、设备号、产品ID和密钥
 * 2. 发送HTTP POST请求到设备认证接口
 * 3. 解析完整JSON响应，提取所有设备信息
 * 4. 如果成功，自动更新AIAssistantManager配置
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

    // 构建JSON请求体 - 与Android端保持一致
    cJSON* root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "deviceNoType", request.deviceNoType.c_str());  // 设备号类型：MAC、SN、IMEI
    cJSON_AddStringToObject(root, "deviceNo", request.deviceNo.c_str());          // 设备号，产品唯一标识设备的序列号
    cJSON_AddStringToObject(root, "productId", request.productId.c_str());        // 产品ID，平台创建产品时生成
    cJSON_AddStringToObject(root, "productKey", request.productKey.c_str());      // 产品密钥，平台创建产品时生成

    // 转换为无格式化的JSON字符串
    char* jsonStr = cJSON_PrintUnformatted(root);
    std::string jsonBody(jsonStr);
    free(jsonStr);  // 释放cJSON分配的内存
    cJSON_Delete(root);  // 释放JSON对象

    // 创建请求头
    std::map<std::string, std::string> headers;
    headers["Content-Type"] = "application/json; charset=utf-8";
    // TODO: Add signature authentication headers
    // 需要添加签名、设备信息等头部，参考Android版本的createAIServerHeaders()

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
 * @brief 处理HTTP响应回调
 *
 * 解析云端返回的JSON数据，提取完整的设备认证信息。
 * 与Android端API保持一致。
 *
 * 响应解析流程：
 * 1. 解析JSON响应字符串
 * 2. 提取status字段（状态码，成功=200）
 * 3. 提取message字段（响应消息或错误信息）
 * 4. 提取data对象中的所有设备信息：
 *    - deviceId：平台上唯一设备标识
 *    - deviceNo：产品内唯一标识设备的序列号
 *    - productId：产品ID
 *    - deviceSecret：设备密钥
 * 5. 释放JSON内存
 * 6. 如果成功，自动更新AIAssistantManager配置
 * 7. 调用用户提供的回调函数
 *
 * 自动配置更新：如果云端返回status=200，SDK会自动更新
 * AIAssistantManager中的设备认证信息，包括deviceId和deviceSecret，
 * 确保后续请求可以使用正确的认证信息。
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

    // 解析code字段 - 请求状态码（成功=200，其他值为异常）
    // 服务器返回的code字段是字符串类型，需要转换为整数
    cJSON* codeJson = cJSON_GetObjectItem(root, "code");
    if (codeJson && cJSON_IsString(codeJson)) {
        deviceInfoResponse.code = std::atoi(codeJson->valuestring);
    } else if (codeJson && cJSON_IsNumber(codeJson)) {
        // 兼容数字类型的code字段
        deviceInfoResponse.code = codeJson->valueint;
    }

    // 解析message字段 - 响应结果，错误提示信息（与Android端保持一致）
    cJSON* messageJson = cJSON_GetObjectItem(root, "message");
    if (messageJson && cJSON_IsString(messageJson)) {
        deviceInfoResponse.message = messageJson->valuestring;
    }

    // 解析data对象 - 包含完整的设备认证信息（与Android端保持一致）
    cJSON* dataJson = cJSON_GetObjectItem(root, "data");
    if (dataJson && cJSON_IsObject(dataJson)) {
        // 解析deviceId - 设备ID，平台上唯一设备标识
        cJSON* deviceIdJson = cJSON_GetObjectItem(dataJson, "deviceId");
        if (deviceIdJson && cJSON_IsString(deviceIdJson)) {
            deviceInfoResponse.data.deviceId = deviceIdJson->valuestring;
        }

        // 解析deviceNo - 设备号，产品内唯一标识设备的序列号
        cJSON* deviceNoJson = cJSON_GetObjectItem(dataJson, "deviceNo");
        if (deviceNoJson && cJSON_IsString(deviceNoJson)) {
            deviceInfoResponse.data.deviceNo = deviceNoJson->valuestring;
        }

        // 解析productId - 产品ID，平台创建产品时生成
        cJSON* productIdJson = cJSON_GetObjectItem(dataJson, "productId");
        if (productIdJson && cJSON_IsString(productIdJson)) {
            deviceInfoResponse.data.productId = productIdJson->valuestring;
        }

        // 解析deviceSecret - 设备密钥，平台创建产品时生成（用于后续请求签名）
        cJSON* deviceSecretJson = cJSON_GetObjectItem(dataJson, "deviceSecret");
        if (deviceSecretJson && cJSON_IsString(deviceSecretJson)) {
            deviceInfoResponse.data.deviceSecret = deviceSecretJson->valuestring;
        }
    }

    // 释放JSON对象内存
    cJSON_Delete(root);

    // 如果成功获取设备信息，自动更新AIAssistantManager的配置
    // 与Android版本保持一致：在RequestApi中直接更新aiAssistConfig
    if (deviceInfoResponse.code == 200) {  // 成功状态码，与服务器返回一致
        if (!deviceInfoResponse.data.deviceId.empty() &&
            !deviceInfoResponse.data.deviceSecret.empty()) {

            // 获取AIAssistantManager的可变配置引用
            auto& config = AIAssistantManager::getInstance().config();

            // 直接赋值更新设备认证信息
            config.deviceId = deviceInfoResponse.data.deviceId;
            config.deviceSecret = deviceInfoResponse.data.deviceSecret;

            ESP_LOGI(TAG, "Device configuration updated in AIAssistantManager:");
            ESP_LOGI(TAG, "  deviceId: %s", deviceInfoResponse.data.deviceId.c_str());
            ESP_LOGI(TAG, "  deviceNo: %s", deviceInfoResponse.data.deviceNo.c_str());
            ESP_LOGI(TAG, "  productId: %s", deviceInfoResponse.data.productId.c_str());
            // 出于安全考虑，不打印deviceSecret到日志
        }
    }

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