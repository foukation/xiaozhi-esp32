#include "ai_sdk/gateway_client.h"
#include "ai_sdk/device_client.h"
#include "ai_sdk/report_client.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char* TAG = "AI_SDK_TEST";

// 测试网关信息获取
static void test_gateway_info() {
    ESP_LOGI(TAG, "=== Testing Gateway Info ===");

    ai_sdk::GatewayClient gateway;

    gateway.getGateWay(
        [](const ai_sdk::GatewayInfo& info, const std::string& message) {
            ESP_LOGI(TAG, "Gateway success:");
            ESP_LOGI(TAG, "  Token: %s", info.token.c_str());
            ESP_LOGI(TAG, "  HTTP URL: %s", info.http_url.c_str());
            ESP_LOGI(TAG, "  WebSocket URL: %s", info.ws_url.c_str());
            ESP_LOGI(TAG, "  Expires in: %d seconds", info.expires_in);
            ESP_LOGI(TAG, "  Status: %d", info.status);
            ESP_LOGI(TAG, "  Message: %s", message.c_str());
        },
        [](const std::string& error) {
            ESP_LOGE(TAG, "Gateway error: %s", error.c_str());
        }
    );
}

// 测试设备信息获取
static void test_device_info() {
    ESP_LOGI(TAG, "=== Testing Device Info ===");

    ai_sdk::DeviceClient device;
    ai_sdk::DeviceInfoRequest request;
    request.deviceNoType = "DEVICE_TYPE_ESP32";
    request.deviceNo = "test_device_001";
    request.productId = "product_123";
    request.productKey = "product_key_abc";

    device.obtainDeviceInformation(
        request,
        [](const ai_sdk::DeviceInfoResponse& response) {
            ESP_LOGI(TAG, "Device auth success:");
            ESP_LOGI(TAG, "  Status: %d", response.status);
            ESP_LOGI(TAG, "  Device ID: %s", response.data.deviceId.c_str());
            ESP_LOGI(TAG, "  Device Secret: %s", response.data.deviceSecret.c_str());
        },
        [](const std::string& error) {
            ESP_LOGE(TAG, "Device auth error: %s", error.c_str());
        }
    );
}

// 测试数据上报
static void test_data_report() {
    ESP_LOGI(TAG, "=== Testing Data Report ===");

    ai_sdk::ReportClient report;
    ai_sdk::DeviceReportRequest request;
    request.eventType = "heartbeat";
    request.params["status"] = "online";
    request.params["battery"] = "85";
    request.params["temperature"] = "25.6";

    report.dataReport(
        request,
        [](const ai_sdk::DeviceReportResponse& response) {
            ESP_LOGI(TAG, "Data report success, status: %d", response.status);
        },
        [](const std::string& error) {
            ESP_LOGE(TAG, "Data report error: %s", error.c_str());
        }
    );
}

// 测试定时上报
static void test_periodic_report() {
    ESP_LOGI(TAG, "=== Testing Periodic Report ===");

    static ai_sdk::ReportClient report;

    // 启动定时上报
    bool started = report.startPeriodicReport([]() {
        ESP_LOGI(TAG, "Executing periodic report...");

        ai_sdk::DeviceReportRequest request;
        request.eventType = "periodic_heartbeat";
        request.params["uptime"] = "3600";
        request.params["memory"] = "1024";

        report.dataReport(
            request,
            [](const ai_sdk::DeviceReportResponse& response) {
                ESP_LOGI(TAG, "Periodic report sent, status: %d", response.status);
            },
            [](const std::string& error) {
                ESP_LOGE(TAG, "Periodic report failed: %s", error.c_str());
            }
        );
    });

    if (started) {
        ESP_LOGI(TAG, "Periodic report started (every 12 hours)");
    } else {
        ESP_LOGE(TAG, "Failed to start periodic report");
    }
}

// AI-SDK 测试入口函数
extern "C" void test_ai_sdk_functions(void)
{
    ESP_LOGI(TAG, "Starting AI-SDK Test...");

    // 等待 Wi-Fi 连接（实际使用时需要先连接 Wi-Fi）
    vTaskDelay(pdMS_TO_TICKS(5000));

    // 运行测试
    test_gateway_info();
    vTaskDelay(pdMS_TO_TICKS(2000));

    test_device_info();
    vTaskDelay(pdMS_TO_TICKS(2000));

    test_data_report();
    vTaskDelay(pdMS_TO_TICKS(2000));

    test_periodic_report();

    ESP_LOGI(TAG, "All tests initiated. Check logs for results.");
}