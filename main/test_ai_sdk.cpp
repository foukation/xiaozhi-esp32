#include "ai_sdk/gateway_client.h"
#include "ai_sdk/device_client.h"
#include "ai_sdk/report_client.h"
#include "esp_log.h"
#include "esp_sntp.h"
#include <ctime>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char* TAG = "AI_SDK_TEST";

// SNTP时间同步任务函数
void sntp_sync_task(void *pvParameters) {
    ESP_LOGI(TAG, "Starting SNTP time synchronization...");

    // 配置SNTP服务器（多服务器确保可靠性）
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
    esp_sntp_setservername(1, "ntp.aliyun.com");
    esp_sntp_setservername(2, "time.windows.com");

    // 设置中国时区
    setenv("TZ", "CST-8", 1);
    tzset();

    // 初始化SNTP
    esp_sntp_init();

    // 等待时间同步完成（最多重试20次）
    int retry_count = 0;
    const int max_retries = 20;

    while (esp_sntp_get_sync_status() != SNTP_SYNC_STATUS_COMPLETED &&
           retry_count < max_retries) {
        ESP_LOGI(TAG, "Waiting for time synchronization... (attempt %d/%d)",
                 ++retry_count, max_retries);
        vTaskDelay(pdMS_TO_TICKS(2000));
    }

    if (esp_sntp_get_sync_status() == SNTP_SYNC_STATUS_COMPLETED) {
        // 获取并打印当前时间
        time_t now;
        time(&now);
        struct tm timeinfo;
        localtime_r(&now, &timeinfo);

        char time_str[32];
        strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", &timeinfo);

        ESP_LOGI(TAG, "Time synchronized successfully!");
        ESP_LOGI(TAG, "Current time: %s", time_str);
    } else {
        ESP_LOGE(TAG, "Failed to synchronize time after %d attempts", max_retries);
    }

    // SNTP会自动定期同步（默认每小时一次）
    ESP_LOGI(TAG, "SNTP sync task completed, will continue periodic sync in background");

    // 删除任务
    vTaskDelete(NULL);
}

/**
 * @brief 等待SNTP时间同步完成
 *
 * @param timeout_ms 超时时间（毫秒），推荐 5000-30000
 *        - 5000: 5秒（网络环境好）
 *        - 10000: 10秒（默认推荐）
 *        - 30000: 30秒（网络环境差）
 * @param check_interval_ms 检查间隔（毫秒），推荐 100-1000
 *        - 100: 100ms（快速检查，但CPU占用稍高）
 *        - 500: 500ms（推荐，平衡实时性和效率）
 *        - 1000: 1秒（节省CPU，但响应稍慢）
 *
 * @return true 同步成功，false 超时失败
 *
 * @details
 * - 启动后最多等待 timeout_ms 毫秒
 * - 每 check_interval_ms 毫秒检查一次同步状态
 * - 同步成功立即返回，不等待完整超时时间
 * - 超时后返回false，但不影响SNTP后台继续运行
 * - 打印详细日志，方便调试和监控
 *
 * @note
 * - 调用此函数前需要先创建SNTP同步任务（xTaskCreate）
 * - 即使返回false，SNTP仍会在后台继续尝试同步
 * - 可以多次调用此函数检查同步状态
 */
bool wait_for_sntp_sync(int timeout_ms, int check_interval_ms)
{
    // 参数校验
    if (timeout_ms <= 0) {
        ESP_LOGE(TAG, "Invalid timeout: %d ms (must be > 0)", timeout_ms);
        return false;
    }

    if (check_interval_ms <= 0) {
        ESP_LOGE(TAG, "Invalid check interval: %d ms (must be > 0)", check_interval_ms);
        return false;
    }

    // 如果检查间隔大于超时时间，调整检查间隔为超时时间
    if (check_interval_ms > timeout_ms) {
        ESP_LOGW(TAG, "Check interval (%d ms) > timeout (%d ms), adjusting interval",
                 check_interval_ms, timeout_ms);
        check_interval_ms = timeout_ms;
    }

    ESP_LOGI(TAG, "开始等待SNTP同步");
    ESP_LOGI(TAG, "超时时间: %d 秒", timeout_ms / 1000);
    ESP_LOGI(TAG, "检查间隔: %d 毫秒", check_interval_ms);

    int elapsed_ms = 0;
    bool sync_success = false;

    while (elapsed_ms < timeout_ms) {
        sntp_sync_status_t sync_status = esp_sntp_get_sync_status();

        if (sync_status == SNTP_SYNC_STATUS_COMPLETED) {
            sync_success = true;

            time_t now;
            time(&now);
            struct tm timeinfo;
            localtime_r(&now, &timeinfo);

            char time_str[32];
            strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", &timeinfo);

            ESP_LOGI(TAG, "✅ SNTP同步成功！");
            ESP_LOGI(TAG, "当前时间: %s", time_str);

            break;
        }

        ESP_LOGI(TAG, "⏳ 等待时间同步中... (%d/%d ms) [状态: %d]",
                 elapsed_ms, timeout_ms, sync_status);

        vTaskDelay(pdMS_TO_TICKS(check_interval_ms));
        elapsed_ms += check_interval_ms;
    }

    if (!sync_success) {
        ESP_LOGW(TAG, "⚠️ SNTP同步超时！已等待 %d 秒", timeout_ms / 1000);
        ESP_LOGW(TAG, "时间可能不准确，但SNTP仍在后台运行");
    }

    return sync_success;
}

static void print_current_time(void) {
    time_t now;
    time(&now);

    struct tm timeinfo;
    localtime_r(&now, &timeinfo);

    char time_str[32];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", &timeinfo);

    ESP_LOGI(TAG, "Current time: %s", time_str);
}

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

/**
 * @brief 定时打印当前时间的任务
 *
 * @param pvParameters 任务参数（未使用）
 *
 * @details
 * - 每10秒打印一次当前系统时间
 * - 使用 FreeRTOS 延时实现精确的时间间隔
 * - 任务会一直运行，直到手动删除
 * - 适合长时间运行的监控和日志记录
 */
static void time_print_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Time print task started");

    while (1) {
        // 打印当前时间
        print_current_time();

        // 等待10秒（pdMS_TO_TICKS将毫秒转换为FreeRTOS时钟节拍）
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}

// AI-SDK 测试入口函数
extern "C" void test_ai_sdk_functions(void)
{
    ESP_LOGI(TAG, "Starting AI-SDK Test...");

    // 等待 Wi-Fi 连接（实际使用时需要先连接 Wi-Fi）
    vTaskDelay(pdMS_TO_TICKS(5000));

    // 启动SNTP时间同步（在测试前确保时间准确）
    // 创建时间同步任务，栈大小4096，优先级4
    // xTaskCreate(sntp_sync_task, "sntp_sync", 4096, NULL, 4, NULL);

    // =========================================
    // 等待SNTP时间同步完成
    // =========================================
    // 使用轮询方式检查同步状态
    // 参数：10秒超时，每500ms检查一次
    bool sync_ok = wait_for_sntp_sync(10000, 500);

    // 根据同步结果打印不同信息
    if (sync_ok) {
        ESP_LOGI(TAG, "✅ 时间同步完成，开始AI-SDK测试");
    } else {
        ESP_LOGW(TAG, "⚠️ 时间同步失败，但仍继续测试（时间戳可能不准确）");
    }
    // =========================================

    // 打印当前时间（同步后的时间，如果同步成功）
    print_current_time();

    // 运行AI-SDK测试
    test_gateway_info();
    vTaskDelay(pdMS_TO_TICKS(2000));

    test_device_info();
    vTaskDelay(pdMS_TO_TICKS(2000));

    test_data_report();
    vTaskDelay(pdMS_TO_TICKS(2000));

    test_periodic_report();

    ESP_LOGI(TAG, "All tests initiated. Check logs for results.");

    // 启动时间打印任务（每10秒输出一次当前时间）
    xTaskCreate(time_print_task, "time_print", 2048, NULL, 3, NULL);
    ESP_LOGI(TAG, "Periodic time output started (every 10 seconds)");

    // 删除测试任务（测试已执行完成，资源可以释放）
    // vTaskDelete(NULL);
}
