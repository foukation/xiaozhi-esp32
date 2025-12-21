#include "ai_sdk/report_client.h"
#include "ai_sdk/api_config.h"
#include "ai_sdk/http_client.h"
#include "cJSON.h"
#include "esp_log.h"

namespace ai_sdk {

static const char* TAG = "ReportClient";

/**
 * 设备数据上报接口
 *
 * 向云端上报设备信息或心跳数据，用于更新设备的最后活动时间。
 *
 * 上报流程：
 * 1. 构建 JSON 请求体，包含 params 参数和 eventType 事件类型
 * 2. 自动添加 SDK 版本号到 params 中
 * 3. 发送 HTTP POST 请求到设备上报接口
 * 4. 解析响应并调用用户回调
 *
 * 使用示例（业务层主动调用）：
 * DeviceReportRequest request;
 * request.eventType = "heartbeat";
 * request.params["status"] = "online";
 * request.params["battery"] = "85";
 * ReportClient client;
 * client.dataReport(request, onSuccess, onError);
 *
 * @param request 上报请求，包含自定义参数和事件类型
 * @param onSuccess 成功回调
 * @param onError 错误回调
 */
void ReportClient::dataReport(
    const DeviceReportRequest& request,
    ReportCallback onSuccess,
    ErrorCallback onError
) {
    // 构建JSON请求体
    cJSON* root = cJSON_CreateObject();

    // 创建params对象 - 用于存放自定义参数
    cJSON* paramsObj = cJSON_CreateObject();
    for (const auto& param : request.params) {
        cJSON_AddStringToObject(paramsObj, param.first.c_str(), param.second.c_str());
    }

    // 自动添加SDK版本号（2.1.0）
    cJSON_AddStringToObject(paramsObj, "sdkVersion", "2.1.0");

    cJSON_AddItemToObject(root, "params", paramsObj);
    cJSON_AddStringToObject(root, "eventType", request.eventType.c_str());

    // 转换为无格式化的JSON字符串
    char* jsonStr = cJSON_PrintUnformatted(root);
    std::string jsonBody(jsonStr);
    free(jsonStr);  // 释放cJSON分配的内存
    cJSON_Delete(root);  // 释放JSON对象

    // 创建请求头
    std::map<std::string, std::string> headers;
    headers["Content-Type"] = "application/json; charset=utf-8";
    // TODO: Add authentication headers
    // 需要添加设备认证信息，参考Android版本

    // 发送HTTP POST请求上报数据
    HTTPClient http_client;
    http_client.post(
        std::string(ApiConfig::TERMINAL_INTELLIGENT_SERVICE_PLATFORM_BASE_URL) + ApiConfig::DEVICE_DATA_REPORT_API,
        jsonBody,
        headers,
        [onSuccess](const std::string& response) {
            ESP_LOGI(TAG, "dataReport response: %s", response.c_str());
            DeviceReportResponse reportResponse;

            // 解析响应（如果需要）
            cJSON* root = cJSON_Parse(response.c_str());
            if (root) {
                cJSON* statusJson = cJSON_GetObjectItem(root, "status");
                if (statusJson && cJSON_IsNumber(statusJson)) {
                    reportResponse.status = statusJson->valueint;
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
