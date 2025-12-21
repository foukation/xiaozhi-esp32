#include "ai_sdk/ai_assistant_manager.h"
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

// AI SDK 管理器实例
static ai_sdk::AIAssistantManager* g_manager = nullptr;

/**
 * @brief 初始化 AI SDK
 *
 * 按照 Android AIAssistantManager 的设计进行初始化：
 * 1. 创建配置（使用 Builder 模式）
 * 2. 初始化管理器
 * 3. 获取实例
 *
 * @return true 初始化成功，false 失败
 */
static bool initialize_ai_sdk() {
    ESP_LOGI(TAG, "Initializing AI SDK...");

    // 1. 创建配置（类似 Android 的 Builder）
    auto builder = std::make_unique<ai_sdk::AIAssistConfig::Builder>();
    auto config = builder->deviceNo("YM00GCDCK01896")
                          .deviceNoType("SN")
                          .productId("1889495584410234882")
                          .productKey("riAtcQzVmPLQprAL")
                          .deviceId("")           // 初始为空，将从云端获取
                          .deviceSecret("")       // 初始为空，将从云端获取
                          .build();

    // 2. 初始化管理器（关键！类似 Android initialize）
    ai_sdk::AIAssistantManager::initialize(nullptr, std::move(config));

    // 3. 获取实例
    try {
        g_manager = &ai_sdk::AIAssistantManager::getInstance();
        ESP_LOGI(TAG, "AI SDK initialized successfully");
        return true;
    } catch (const std::exception& e) {
        ESP_LOGE(TAG, "Failed to get AI SDK instance: %s", e.what());
        return false;
    }
}

/**
 * @brief 测试网关访问（手动调用）
 */
static void test_gateway_access() {
    ESP_LOGI(TAG, "=== Testing Gateway Access ===");

    if (!g_manager) {
        ESP_LOGE(TAG, "AI SDK not initialized!");
        return;
    }

    // 通过 gateWayHelp() 获取网关对象，然后调用 getGateWay()
    // 这与 Android 的调用方式完全一致：manager.gateWayHelp().getGateWay()
    g_manager->gateWayHelp().getGateWay(
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

/**
 * @brief 测试数据上报（手动调用）
 */
static void test_data_report() {
    ESP_LOGI(TAG, "=== Testing Data Report ===");

    if (!g_manager) {
        ESP_LOGE(TAG, "AI SDK not initialized!");
        return;
    }

    // 构建上报请求
    ai_sdk::DeviceReportRequest request;
    request.eventType = "heartbeat";
    request.params["status"] = "online";
    request.params["battery"] = "85";
    request.params["temperature"] = "25.6";

    // 通过 gateWayHelp() 调用 dataReport()
    // 这与 Android 的调用方式完全一致：manager.gateWayHelp().dataReport()
    g_manager->gateWayHelp().dataReport(
        request,
        [](const ai_sdk::DeviceReportResponse& response) {
            ESP_LOGI(TAG, "Data report success, status: %d", response.status);
        },
        [](const std::string& error) {
            ESP_LOGE(TAG, "Data report error: %s", error.c_str());
        }
    );
}

/**
 * @brief 测试设备信息获取（通过 GateWay）
 *
 * 通过 gateWayHelp() 访问网关对象，然后调用 obtainDeviceInformation()
 * 这与 Android 的调用方式完全一致：manager.gateWayHelp().obtainDeviceInformation()
 *
 * 重要说明：
 * - 不能直接调用 g_manager->obtainDeviceInformation()（该方法已在 AIAssistantManager 中删除）
 * - 必须通过 gateWayHelp() 访问 GateWay 对象
 * - 回调函数处理成功和错误情况
 *
 * 参考 Android: manager.gateWayHelp().obtainDeviceInformation(onSuccess, onError)
 */
static void test_device_information() {
    ESP_LOGI(TAG, "=== Testing Device Information ===");

    if (!g_manager) {
        ESP_LOGE(TAG, "AI SDK not initialized!");
        return;
    }

    // 通过 gateWayHelp() 调用 obtainDeviceInformation()
    // 这与 Android 的调用方式完全一致
    g_manager->gateWayHelp().obtainDeviceInformation(
        [](const ai_sdk::DeviceInfoResponse& response) {
            ESP_LOGI(TAG, "Device info success:");
            ESP_LOGI(TAG, "  Status: %d", response.status);
            ESP_LOGI(TAG, "  Device ID: %s", response.data.deviceId.c_str());
            ESP_LOGI(TAG, "  Device Secret: %s", response.data.deviceSecret.c_str());

        // 测试 2: 网关访问（手动调用）
        ESP_LOGI(TAG, "Test 2: Gateway Access");
        test_gateway_access();
        vTaskDelay(pdMS_TO_TICKS(3000));

        // 测试 3: 数据上报（手动调用）
        ESP_LOGI(TAG, "Test 3: Data Report");
        test_data_report();
        vTaskDelay(pdMS_TO_TICKS(3000));

        },
        [](const std::string& error) {
            ESP_LOGE(TAG, "Device info error: %s", error.c_str());
        }
    );

    ESP_LOGI(TAG, "Device information request sent, check logs for results");
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

/**
 * @brief AI SDK 测试任务（核心测试流程）
 *
 * 该任务演示了如何使用新的 AI SDK（与 Android 设计完全一致）：
 *
 * 测试步骤：
 * 1. 等待时间同步（ESP32 需要准确的时间戳）
 * 2. 初始化 AI SDK（使用 AIAssistantManager::initialize）
 * 3. 测试完整的设备启动流程（调用 obtainDeviceInformation）
 * 4. 可选择测试单独的功能
 *
 * 重要说明：
 * - 本任务完全遵循 Android AIAssistantManager 的使用模式
 * - 所有操作都通过 AIAssistantManager 实例完成
 * - 网关操作统一通过 gateWayHelp() 访问
 * - 这是与 Android 项目保持一致的示例代码
 *
 * 与旧版的区别：
 * - ❌ 旧版：直接创建 GatewayClient、DeviceClient、ReportClient
 * - ✅ 新版：通过 AIAssistantManager::gateWayHelp() 统一访问
 *
 * @param arg 任务参数（未使用）
 */
void ai_sdk_test_task(void* arg) {
    ESP_LOGI(TAG, "AI SDK test task started");

    // ============================================
    // 步骤 1: 等待时间同步完成
    // ============================================
    // ESP32 需要准确的时间戳，因为：
    // 1. SSL/TLS 证书验证需要准确时间
    // 2. 上报数据的时间戳需要准确
    // 3. 定时任务调度需要准确时间
    ESP_LOGI(TAG, "Waiting for time synchronization...");
    int wait_seconds = 0;
    while (wait_seconds < 20) {
        time_t now;
        time(&now);

        // 检查时间是否已同步（如果时间 > 2021年，说明已同步）
        if (now > 1609459200) {
            ESP_LOGI(TAG, "Time synchronization completed!");
            break;
        }

        ESP_LOGI(TAG, "Waiting... %d seconds", wait_seconds);
        vTaskDelay(pdMS_TO_TICKS(1000));
        wait_seconds++;
    }

    // 处理时间同步超时的情况
    if (wait_seconds >= 20) {
        ESP_LOGW(TAG, "SNTP sync timeout, but continuing test (timestamps may be inaccurate)");
    }

    // 打印当前时间（用于验证时间同步）
    print_current_time();
    ESP_LOGI(TAG, "Time sync step completed");

    // ============================================
    // 步骤 2: 初始化 AI SDK
    // ============================================
    // 这是关键改变点！现在使用 AIAssistantManager 统一管理
    // 类似 Android: AIAssistantManager.initialize(context, config)
    if (!initialize_ai_sdk()) {
        ESP_LOGE(TAG, "AI SDK initialization failed! Cannot proceed with tests.");
        vTaskDelete(NULL);
        return;
    }
    ESP_LOGI(TAG, "AI SDK initialization completed");

    // 等待一小段时间，确保初始化完成
    vTaskDelay(pdMS_TO_TICKS(1000));

    // ============================================
    // 步骤 3: 测试 GateWay 功能
    // ============================================
    // 这些功能都通过 gateWayHelp() 访问，与 Android 完全一致
    // 注意：AIAssistantManager 不再直接提供这些方法
    // 必须通过 manager.gateWayHelp().xxx() 调用
    ESP_LOGI(TAG, "Testing GateWay features...");

    // 测试 1: 设备信息获取（通过 gateWayHelp）
    ESP_LOGI(TAG, "Test 1: Device Information");
    test_device_information();
    vTaskDelay(pdMS_TO_TICKS(3000));

    // 这些功能也可以手动调用，用于调试或特殊场景
    // 注释掉的部分可以根据需要启用
    ESP_LOGI(TAG, "Optional individual feature tests (commented out)");

    // ============================================
    // 测试完成
    // ============================================
    ESP_LOGI(TAG, "All test steps completed! Check logs above for results.");
    vTaskDelete(NULL);
}

/**
 * @brief AI-SDK 测试入口函数（主入口）
 *
 * 这是 AI SDK 测试的入口函数，演示了如何像 Android 一样使用 AI SDK。
 *
 * 核心特点：
 * - 完全遵循 Android AIAssistantManager 的设计模式
 * - 统一通过 AIAssistantManager 管理所有 AI 功能
 * - 网关操作统一通过 gateWayHelp() 访问
 * - 初始化流程：initialize() -> getInstance() -> 调用功能
 *
 * 调用示例：
 * // 1. 创建配置
 * auto builder = std::make_unique<AIAssistConfig::Builder>();
 * auto config = builder->deviceNo("device-001")
 *                      ->productId("PROD001")
 *                      ->productKey("KEY001")
 *                      ->build();
 *
 * // 2. 初始化管理器（类似 Android：AIAssistantManager.initialize(context, config)）
 * AIAssistantManager::initialize(nullptr, std::move(config));
 *
 * // 3. 获取实例并调用方法
 * auto& manager = AIAssistantManager::getInstance();
 * manager.obtainDeviceInformation();  // 会自动调用 getGateWay() -> dataReport()
 *
 * 日志输出：
 * - 启动日志（表明测试开始）
 * - 时间同步日志
 * - SDK 初始化日志
 * - 设备注册日志
 * - 网关配置日志
 * - 数据上报日志
 * - 定时时间输出（每10秒）
 *
 * 注意事项：
 * - 在实际设备上运行前，需要配置正确的 productId 和 productKey
 * - 需要先连接 WiFi（时间同步和网络通信需要）
 * - 栈大小设置为 8192，确保有足够的内存用于证书和网络操作
 */
extern "C" void test_ai_sdk_functions(void)
{
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "Starting AI-SDK Test (Android Compatible)");
    ESP_LOGI(TAG, "========================================");

    // 打印当前时间（用于验证时间同步）
    print_current_time();
    ESP_LOGI(TAG, "Current time printed");

    // ========================================
    // 创建测试任务
    // ========================================
    // 栈大小设置为 8192 字节的原因：
    // 1. SSL/TLS 握手需要大量内存
    // 2. JSON 解析需要额外栈空间
    // 3. 网络缓冲区分配
    // 4. 回调函数调用链
    ESP_LOGI(TAG, "Creating AI SDK test task (stack: 8192 bytes, priority: 5)");
    xTaskCreate(ai_sdk_test_task, "ai_sdk_test", 8192, NULL, 5, NULL);
    ESP_LOGI(TAG, "AI SDK test task created successfully");

    // ========================================
    // 创建时间打印任务（可选，用于监控）
    // ========================================
    // 每 10 秒打印一次当前时间，用于：
    // 1. 验证时间同步是否正常
    // 2. 检查系统是否正常运行
    // 3. 监控任务调度
    ESP_LOGI(TAG, "Creating time monitor task (stack: 2048 bytes, priority: 3)");
    xTaskCreate(time_print_task, "time_print", 2048, NULL, 3, NULL);
    ESP_LOGI(TAG, "Time monitor task created successfully");

    // ========================================
    // 测试启动完成
    // ========================================
    ESP_LOGI(TAG, "Test framework initialized. Check logs above for results.");
    ESP_LOGI(TAG, "Expected log sequence:");
    ESP_LOGI(TAG, "  1. Time synchronization");
    ESP_LOGI(TAG, "  2. AI SDK initialization");
    ESP_LOGI(TAG, "  3. Device registration (obtainDeviceInformation)");
    ESP_LOGI(TAG, "  4. Gateway configuration (getGateWay)");
    ESP_LOGI(TAG, "  5. Initial data report (dataReport)");
    ESP_LOGI(TAG, "  6. Periodic time output (every 10 seconds)");
}
