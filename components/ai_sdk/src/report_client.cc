#include "ai_sdk/report_client.h"
#include "ai_sdk/api_config.h"
#include "ai_sdk/http_client.h"
#include "cJSON.h"
#include "esp_log.h"
#include <cstdlib>
#include <random>

namespace ai_sdk {

static const char* TAG = "ReportClient";

ReportClient::ReportClient() : report_callback_(), is_reporting_(false), timer_handle_(nullptr) {
}

ReportClient::~ReportClient() {
    // 析构时停止定时上报
    stopPeriodicReport();
}

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

bool ReportClient::startPeriodicReport(std::function<void()> reportCallback) {
    // 检查是否已经启动
    if (is_reporting_) {
        ESP_LOGW(TAG, "Periodic report already started");
        return false;
    }

    // 保存回调函数
    report_callback_ = reportCallback;
    is_reporting_ = true;

    // 创建定时器
    esp_timer_create_args_t timer_args = {};
    timer_args.callback = timerCallback;  // 定时器回调函数
    timer_args.name = "report_timer";      // 定时器名称
    timer_args.arg = this;                 // 传递this指针

    esp_err_t ret = esp_timer_create(&timer_args, &timer_handle_);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create timer: %s", esp_err_to_name(ret));
        is_reporting_ = false;
        return false;
    }

    // 调度下一次上报
    scheduleNextReport();

    ESP_LOGI(TAG, "Periodic report started");
    return true;
}

void ReportClient::stopPeriodicReport() {
    // 检查是否正在运行
    if (!is_reporting_) {
        return;
    }

    is_reporting_ = false;

    // 停止并删除定时器
    if (timer_handle_) {
        esp_timer_stop(timer_handle_);
        esp_timer_delete(timer_handle_);
        timer_handle_ = nullptr;
    }

    ESP_LOGI(TAG, "Periodic report stopped");
}

void ReportClient::timerCallback(void* arg) {
    ReportClient* self = static_cast<ReportClient*>(arg);

    // 检查是否还在运行状态
    if (self->is_reporting_ && self->report_callback_) {
        ESP_LOGI(TAG, "Executing periodic report");
        // 执行用户提供的上报回调
        self->report_callback_();

        // 调度下一次上报
        self->scheduleNextReport();
    }
}

void ReportClient::scheduleNextReport() {
    if (!is_reporting_ || !timer_handle_) {
        return;
    }

    // 计算下次上报时间：12小时 + 随机偏移（-15到+15分钟）
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(-900000000, 900000000); // -15分钟到+15分钟（微秒）

    int64_t base_interval = 12LL * 60 * 60 * 1000000; // 12小时（微秒）
    int64_t random_offset = dis(gen);
    int64_t next_interval = base_interval + random_offset;

    ESP_LOGI(TAG, "Next report scheduled in %lld seconds", next_interval / 1000000);

    // 启动一次性定时器
    esp_err_t ret = esp_timer_start_once(timer_handle_, next_interval);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start timer: %s", esp_err_to_name(ret));
    }
}

} // namespace ai_sdk