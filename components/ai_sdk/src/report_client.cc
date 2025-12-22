#include "ai_sdk/report_client.h"
#include "ai_sdk/api_config.h"
#include "ai_sdk/http_client.h"
#include "cJSON.h"
#include "esp_log.h"

namespace ai_sdk {

static const char* TAG = "ReportClient";

/**
 * @brief 设备数据上报接口
 *
 * 向云端上报设备信息或心跳数据，用于更新设备的最后活动时间。
 * 与Android端API保持一致。
 *
 * 上报流程：
 * 1. 构建 JSON 请求体，包含设备认证信息和参数
 * 2. 自动添加 SDK 版本号到 params 中
 * 3. 发送 HTTP POST 请求到设备上报接口
 * 4. 解析完整响应并调用用户回调
 *
 * 请求结构（与Android端一致）：
 * {
 *   "deviceId": "设备唯一标识",
 *   "deviceSecret": "设备密钥",
 *   "productId": "产品ID",
 *   "productKey": "产品密钥",
 *   "params": {
 *     "innerIp": ["内网IP"],
 *     "netSpeed": "网络分级",
 *     "netType": "网络类型",
 *     "platform": "操作系统",
 *     "sdkVersion": "SDK版本",
 *     "firmwareVersion": "固件版本",
 *     "mac": "MAC地址"
 *   }
 * }
 *
 * 响应结构（与Android端一致）：
 * {
 *   "status": 200,
 *   "message": "响应消息",
 *   "data": {
 *     "deviceId": "设备ID",
 *     "protocolTypeTime": "协议类型时间"
 *   }
 * }
 *
 * @param request 上报请求，包含设备认证信息和参数
 * @param onSuccess 成功回调，返回完整响应信息
 * @param onError 错误回调
 */
void ReportClient::dataReport(
    const DeviceReportRequest& request,
    ReportCallback onSuccess,
    ErrorCallback onError
) {
    ESP_LOGI(TAG, "API: dataReport");

    // 构建JSON请求体 - 与Android端保持一致
    cJSON* root = cJSON_CreateObject();

    // 添加设备认证信息 - 必需字段
    cJSON_AddStringToObject(root, "deviceId", request.deviceId.c_str());
    cJSON_AddStringToObject(root, "deviceSecret", request.deviceSecret.c_str());
    cJSON_AddStringToObject(root, "productId", request.productId.c_str());
    cJSON_AddStringToObject(root, "productKey", request.productKey.c_str());

    // 创建params对象 - 用于存放自定义参数
    cJSON* paramsObj = cJSON_CreateObject();

    // 添加自定义参数
    for (const auto& param : request.params) {
        // 支持不同类型的参数值（string, number, boolean, array）
        try {
            // 尝试转换为字符串（如果原类型是std::any）
            if (param.second.type() == typeid(std::string)) {
                std::string value = std::any_cast<std::string>(param.second);
                cJSON_AddStringToObject(paramsObj, param.first.c_str(), value.c_str());
            } else if (param.second.type() == typeid(int)) {
                int value = std::any_cast<int>(param.second);
                cJSON_AddNumberToObject(paramsObj, param.first.c_str(), value);
            } else if (param.second.type() == typeid(double)) {
                double value = std::any_cast<double>(param.second);
                cJSON_AddNumberToObject(paramsObj, param.first.c_str(), value);
            } else if (param.second.type() == typeid(bool)) {
                bool value = std::any_cast<bool>(param.second);
                cJSON_AddBoolToObject(paramsObj, param.first.c_str(), value);
            } else {
                // 默认转换为字符串
                cJSON_AddStringToObject(paramsObj, param.first.c_str(), "unknown_type");
            }
        } catch (const std::bad_any_cast& e) {
            ESP_LOGW(TAG, "Failed to convert param value for key: %s", param.first.c_str());
            cJSON_AddStringToObject(paramsObj, param.first.c_str(), "conversion_error");
        }
    }

    // 自动添加SDK版本号（与Android端保持一致）
    cJSON_AddStringToObject(paramsObj, "sdkVersion", "ai_2.1.0");

    cJSON_AddItemToObject(root, "params", paramsObj);

    // 转换为无格式化的JSON字符串
    char* jsonStr = cJSON_PrintUnformatted(root);
    std::string jsonBody(jsonStr);
    free(jsonStr);  // 释放cJSON分配的内存
    cJSON_Delete(root);  // 释放JSON对象

    // 输出请求参数日志（与Android OkHttpManager保持一致）
    ESP_LOGI(TAG, "request params: %s", jsonBody.c_str());

    // 创建请求头
    std::map<std::string, std::string> headers;
    headers["Content-Type"] = "application/json; charset=utf-8";
    // TODO: Add signature authentication headers
    // 需要使用deviceSecret生成签名，参考Android版本的认证机制

    // 发送HTTP POST请求上报数据
    HTTPClient http_client;
    http_client.post(
        std::string(ApiConfig::TERMINAL_INTELLIGENT_SERVICE_PLATFORM_BASE_URL) + ApiConfig::DEVICE_DATA_REPORT_API,
        jsonBody,
        headers,
        [onSuccess](const std::string& response) {
            ESP_LOGI(TAG, "dataReport response: %s", response.c_str());
            DeviceReportResponse reportResponse;

            // 解析完整响应 - 与Android端保持一致
            cJSON* root = cJSON_Parse(response.c_str());
            if (root) {
                // 解析状态码
                cJSON* statusJson = cJSON_GetObjectItem(root, "status");
                if (statusJson && cJSON_IsNumber(statusJson)) {
                    reportResponse.status = statusJson->valueint;
                }

                // 解析响应消息
                cJSON* messageJson = cJSON_GetObjectItem(root, "message");
                if (messageJson && cJSON_IsString(messageJson)) {
                    reportResponse.message = messageJson->valuestring;
                }

                // 解析数据对象
                cJSON* dataJson = cJSON_GetObjectItem(root, "data");
                if (dataJson && cJSON_IsObject(dataJson)) {
                    // 解析设备ID
                    cJSON* deviceIdJson = cJSON_GetObjectItem(dataJson, "deviceId");
                    if (deviceIdJson && cJSON_IsString(deviceIdJson)) {
                        reportResponse.data.deviceId = deviceIdJson->valuestring;
                    }

                    // 解析协议类型时间
                    cJSON* protocolTypeTimeJson = cJSON_GetObjectItem(dataJson, "protocolTypeTime");
                    if (protocolTypeTimeJson && cJSON_IsString(protocolTypeTimeJson)) {
                        reportResponse.data.protocolTypeTime = protocolTypeTimeJson->valuestring;
                    }
                }

                cJSON_Delete(root);
            }

            // 调用成功回调
            if (onSuccess) {
                onSuccess(reportResponse);
            }
        },
        [onError](const std::string& error) {
            ESP_LOGE(TAG, "dataReport failed: %s", error.c_str());
            // 调用错误回调
            if (onError) {
                onError(error);
            }
        }
    );
}

} // namespace ai_sdk
