/**
 * @file gate_way.cc
 * @brief GateWay 类实现文件
 *
 * 该文件实现了 GateWay 类的所有方法，完全参考 Android com.cmdc.ai.assist.api.GateWay 的实现。
 * GateWay 提供网关配置、设备信息获取和数据上报的统一入口。
 *
 * 重要设计说明：
 * - GateWay 是一个薄薄的转发层，类似 Android 的设计
 * - 实际的 HTTP 逻辑、重试机制、错误处理都在 Client 类中实现
 * - 这种设计保持与 Android 的一致性，同时复用 ESP32 已有的 Client 实现
 * - 如果未来需要，可以在 GateWay 层添加统一的重试、缓存、监控等功能
 */

#include "ai_sdk/gate_way.h"
#include "esp_log.h"
#include <string>

// 包含已有的 Client 实现
// GateWay 的核心职责是转发调用到这些已经实现并测试过的 Client 类
// - GatewayClient: 处理网关/代理配置相关的网络请求
// - DeviceClient: 处理设备注册认证相关的网络请求
// - ReportClient: 处理数据上报和心跳相关的网络请求
#include "ai_sdk/gateway_client.h"   // 网关客户端，提供 getGateWay() 实现
#include "ai_sdk/device_client.h"    // 设备客户端，提供 obtainDeviceInformation() 实现
#include "ai_sdk/report_client.h"    // 上报客户端，提供 dataReport() 实现

namespace ai_sdk {

/**
 * @brief 日志标签
 */
static const char* TAG = "GateWay";

/**
 * @brief 获取网关信息
 *
 * 从云端获取网关配置，包括代理服务器地址和认证令牌。
 *
 * 实现说明（重要）：
 * 这是一个典型的转发调用设计，完全遵循 Android GateWay 的模式。
 * 与 Android 不同，ESP32 已有 GatewayClient 实现了完整的逻辑,
 * 因此 GateWay 不需要重复实现 HTTP 请求、重试、错误处理等功能。
 *
 * 转发流程：
 * 1. 创建 GatewayClient 实例（栈上分配，自动销毁）
 * 2. 直接调用 client.getGateWay() 并透传回调函数
 * 3. Client 处理所有网络请求、解析、错误重试等复杂逻辑
 * 4. 获得结果后，Client 会调用传入的 onSuccess/onError 回调通知 GateWay
 * 5. GateWay 不做任何干预，让结果直接返回给调用者
 *
 * 这种设计的优势：
 * - 保持与 Android 一致的 API 设计（AIAssistantManager.gateWayHelp().xxx()）
 * - 复用 ESP32 已有的稳定实现（GatewayClient 已经过测试）
 * - 代码清晰：GateWay 只做转发，Client 做具体实现
 * - 易于维护：所有网络逻辑集中在 Client 类中
 *
 * 应用场景：
 * - 设备首次启动时获取代理配置
 * - 代理令牌过期时重新获取
 * - 根据网络环境选择合适的代理服务器
 *
 * 成功后，SDK 会自动更新全局 ApiConfig 配置，后续请求会自动使用代理。
 *
 * @param onSuccess 成功回调，返回网关信息（代理地址、令牌、有效期）
 * @param onError 错误回调，返回错误信息
 */
void GateWay::getGateWay(
    GatewaySuccessCallback onSuccess,
    GatewayErrorCallback onError
) {
    ESP_LOGI(TAG, "Getting gateway configuration...");

    // 转发调用到 GatewayClient
    // 这是方案 A 的核心：GateWay 作为薄薄的一层，调用已有的 Client 实现
    // GatewayClient 已经实现了完整的 HTTP 请求、解析响应、错误处理等逻辑
    // 我们不需要重复实现，只需创建 Client 并调用它
    GatewayClient client;  // 栈上分配，函数结束后自动销毁

    // 直接透传回调函数 - 这是一个关键设计点：
    // 我们让调用者的回调直接传递到 GatewayClient
    // 这样当 GatewayClient 完成后，回调会直接通知原始调用者
    // GateWay 本身不做任何拦截或处理，保持了调用的透明度
    client.getGateWay(onSuccess, onError);

    // 注意：这里没有错误处理，因为所有错误都通过 onError 回调返回
    // 这是异步编程的最佳实践：错误不通过返回值，而是通过回调通知
}

/**
 * @brief 获取设备信息
 *
 * 设备通过产品信息及设备号从云端获取设备信息，前提是设备号已录入到云端平台。
 *
 * 实现说明（与 Android 保持一致的关键修改）：
 * 这是从 Client 获取配置的第 2 步：删除成员变量，每次从 AIAssistantManager 获取。
 *
 * 设计演进：
 * - 第 1 版（错误）：GateWay 通过成员变量保存配置指针
 *   - 问题：与 Android 设计不一致，增加了 GateWay 的状态
 *   - 问题：需要 setConfig() 方法，API 不统一
 * - 第 2 版（当前）：每次从 AIAssistantManager::getInstance() 获取配置
 *   - 与 Android 完全一致：Android 的 RequestApi 也是从 AIAssistantManager 获取配置
 *   - 优势：GateWay 完全无状态，职责单一（只做转发）
 *   - 优势：不需要 setConfig()，减少了 API 和初始化步骤
 *
 * 与 Android 的对比：
 * Android (Kotlin):
 *   fun obtainDeviceInformation(onSuccess, onError) {
 *       val deviceInfoRequest = DeviceInfoRequest(
 *           deviceNoType = AIAssistantManager.getInstance().aiAssistConfig.deviceNoType,
 *           deviceNo = AIAssistantManager.getInstance().aiAssistConfig.deviceNo,
 *           ...
 *       )
 *       // 发送请求...
 *   }
 *
 * ESP32 (C++):
 *   void obtainDeviceInformation(onSuccess, onError) {
 *       DeviceInfoRequest request;
 *       request.deviceNoType = AIAssistantManager::getInstance().config().deviceNoType;
 *       request.deviceNo = AIAssistantManager::getInstance().config().deviceNo;
 *       // 发送请求...
 *   }
 *
 * 两者核心一致：都从中央管理器（AIAssistantManager）获取配置，而不是存储在成员变量中。
 *
 * 转发流程：
 * 1. 从 AIAssistantManager::getInstance().config() 获取配置（每次调用都获取）
 * 2. 从配置中提取设备信息和产品信息
 * 3. 构建 DeviceInfoRequest 对象（栈上分配）
 * 4. 创建 DeviceClient 实例（栈上分配，自动销毁）
 * 5. 调用 client.obtainDeviceInformation(request, onSuccess, onError)
 * 6. Client 处理所有网络请求、解析响应、错误重试等复杂逻辑
 * 7. 结果通过 onSuccess/onError 回调返回给调用者
 *
 * 使用回调函数来处理成功和错误的情况
 *
 * @param onSuccess 成功回调，返回设备信息（deviceId 和 deviceSecret）
 * @param onError 错误回调，返回错误信息
 */
void GateWay::obtainDeviceInformation(
    DeviceInfoSuccessCallback onSuccess,
    DeviceInfoErrorCallback onError
) {
    ESP_LOGI(TAG, "Obtaining device information...");

    // 从 AIAssistantManager 获取配置（关键修改点）
    // 与 Android 保持一致：每次调用都从 AIAssistantManager 获取，不存储成员变量
    // 这样做的好处：
    // 1. GateWay 完全无状态，职责单一（只做转发）
    // 2. 与 Android 设计完全一致
    // 3. 不需要 setConfig() 方法，减少 API 复杂度
    const auto& config = AIAssistantManager::getInstance().config();

    // 从配置构建请求参数
    DeviceInfoRequest request;
    request.deviceNoType = config.deviceNoType;  // 设备号类型（如 "SN"）
    request.deviceNo = config.deviceNo;          // 设备序列号
    request.productId = config.productId;        // 产品 ID（从云端平台获得）
    request.productKey = config.productKey;      // 产品密钥（从云端平台获得）

    ESP_LOGI(TAG, "Building DeviceInfoRequest from AIAssistantManager::config():");
    ESP_LOGI(TAG, "  deviceNoType: %s", request.deviceNoType.c_str());
    ESP_LOGI(TAG, "  deviceNo: %s", request.deviceNo.c_str());
    ESP_LOGI(TAG, "  productId: %s", request.productId.c_str());
    // 注意：productKey 不打印到日志，避免泄露敏感信息

    // 转发调用到 DeviceClient
    // 这是方案 A 的核心：GateWay 作为薄薄的一层，调用已有的 Client 实现
    // DeviceClient 已经实现了完整的 HTTP 请求、JSON 解析、错误处理、重试等逻辑
    // 我们不需要重复实现，只需创建 Client 并调用它
    DeviceClient client;  // 栈上分配，函数结束后自动销毁

    // 调用 DeviceClient 的 obtainDeviceInformation()
    // 注意：我们传递刚刚构建的 request、以及调用者提供的回调函数
    // 这样当 DeviceClient 完成后，回调会直接通知原始调用者
    // GateWay 本身不做任何拦截或处理，保持了调用的透明度和 API 的一致性
    client.obtainDeviceInformation(request, onSuccess, onError);

    ESP_LOGI(TAG, "DeviceInfoRequest forwarded to DeviceClient");

    // 注意：这里没有错误处理，因为所有错误（包括网络错误、认证错误等）
    // 都通过 onError 回调返回。这是异步编程的最佳实践。
}

/**
 * @brief 数据上报
 *
 * 心跳接口/上报接口（定时向云端发送消息）
 * 设备（24小时至少上报一次）向云端上报信息，更新最后活动时间。
 *
 * 实现说明（重要）：
 * 这是方案 A 的第三个转发调用实现。
 * 与 obtainDeviceInformation() 类似，但更简单，因为请求参数已经由调用者构建。
 *
 * 转发流程：
 * 1. 调用者构建 DeviceReportRequest（包含 eventType、params 等）
 * 2. 接收调用者传入的 deviceReportRequest 参数（无需 GateWay 自己构建）
 * 3. 创建 ReportClient 实例（栈上分配，自动销毁）
 * 4. 调用 client.dataReport(deviceReportRequest, onSuccess, onError)
 * 5. Client 处理所有网络请求、JSON 序列化、错误处理、重试等复杂逻辑
 * 6. 结果通过 onSuccess/onError 回调返回给调用者
 *
 * 这种设计的优势：
 * - 保持与 Android 一致的 API 设计（AIAssistantManager.gateWayHelp().dataReport()）
 * - 复用 ESP32 已有的稳定实现（ReportClient 已经过测试）
 * - 代码清晰：GateWay 只做转发，Client 做具体实现
 * - 参数构建灵活：调用者可以自由构造 DeviceReportRequest，上传任何数据
 * - 易于维护：所有网络逻辑集中在 Client 类中
 *
 * 请求策略（参考）：
 * 设备应该每隔12小时向平台上报设备数据信息。
 * 实施步骤：
 * 1. 设备初始化：设备首次启动时，向平台上报数据并记录上报时间 T_current。
 * 2. 计算下次上报时间：设备每次上报数据后，记录当前时间 T_current，并计算下次上报时间 T_next：
 *    T_next = T_current + 12 小时 + 随机偏移量
 *    其中，随机偏移量可以是在-15分钟到+15分钟之间的一个随机值。
 * 3. 调度上报任务：设备根据计算出的 T_next 设置定时任务，确保在该时间点上报数据。
 * 注意：避免固定时间集中上报，造成服务器压力过大。
 *
 * 与 obtainDeviceInformation() 的区别：
 * - obtainDeviceInformation()：GateWay 需要从 config_ 构建请求（需要配置访问）
 * - dataReport()：调用者直接传入完整的 deviceReportRequest（无需配置访问）
 * - 因此 dataReport() 实现更简单，代码更少
 *
 * @param deviceReportRequest 包含设备报告请求的数据对象（由调用者构建）
 * @param onSuccess 成功回调，当数据上报成功时调用
 * @param onError 错误回调，当数据上报失败时调用
 */
void GateWay::dataReport(
    const DeviceReportRequest& deviceReportRequest,
    ReportSuccessCallback onSuccess,
    ReportErrorCallback onError
) {
    ESP_LOGI(TAG, "Sending data report...");

    // 转发调用到 ReportClient
    // 这是方案 A 的核心：GateWay 作为薄薄的一层，调用已有的 Client 实现
    // ReportClient 已经实现了完整的 HTTP 请求、JSON 序列化、错误处理、重试等逻辑
    // 我们不需要重复实现，只需创建 Client 并调用它
    ReportClient client;  // 栈上分配，函数结束后自动销毁

    // 直接透传回调函数和请求参数 - 这是关键设计点：
    // - deviceReportRequest：调用者已经构建好的上报数据，直接透传
    // - onSuccess/onError：调用者的回调函数，直接透传
    // 这样当 ReportClient 完成后，回调会直接通知原始调用者
    // GateWay 本身不做任何拦截、修改或处理，保持了调用的透明度和 API 的一致性
    client.dataReport(deviceReportRequest, onSuccess, onError);

    ESP_LOGI(TAG, "DeviceReportRequest forwarded to ReportClient");

    // 注意：这里没有错误处理，因为所有错误都通过 onError 回调返回
    // 这是异步编程的最佳实践：错误不通过返回值，而是通过回调通知
}

/**
 * @brief HTTP响应处理回调 - 设备信息
 */
void GateWay::onDeviceInfoResponse(const std::string& response,
                                   DeviceInfoSuccessCallback onSuccess,
                                   DeviceInfoErrorCallback onError) {
    // TODO: 实现响应解析
    if (onError) {
        onError("Not implemented");
    }
}

/**
 * @brief HTTP响应处理回调 - 网关信息
 */
void GateWay::onGatewayResponse(const std::string& response,
                                GatewaySuccessCallback onSuccess,
                                GatewayErrorCallback onError) {
    // TODO: 实现响应解析
    if (onError) {
        onError("Not implemented");
    }
}

/**
 * @brief HTTP响应处理回调 - 上报
 */
void GateWay::onReportResponse(const std::string& response,
                               ReportSuccessCallback onSuccess,
                               ReportErrorCallback onError) {
    // TODO: 实现响应解析
    if (onError) {
        onError("Not implemented");
    }
}

} // namespace ai_sdk
