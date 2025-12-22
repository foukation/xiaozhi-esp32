#pragma once

#include "ai_sdk/types.h"
#include <functional>

namespace ai_sdk {

/**
 * @brief 上报客户端类
 *
 * 用于设备数据上报，提供即时上报功能。
 * 与Android端API保持一致。
 *
 * 设计说明：
 * - 本类只提供即时上报能力（dataReport），不提供定时上报功能
 * - 定时上报是业务层的功能，SDK 只提供基础的 HTTP 上报接口
 * - 业务层可以根据需要自行实现定时上报（如使用 FreeRTOS 定时器、esp_timer 等）
 * - 这种设计与 Android SDK 保持一致：Android 的 ReportClient 也不提供定时上报
 *
 * 功能特性：
 * - 即时上报：通过 dataReport() 接口立即上报设备数据
 * - 简单易用：无状态设计，无需初始化，即调即用
 * - 灵活可控：业务层完全控制上报时机和频率
 * - 完整响应：解析完整的响应数据，包括状态码、消息和数据对象
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
 */
class ReportClient {
public:
    /**
     * @brief 上报成功回调类型
     * @param response 上报响应，包含完整的响应信息
     */
    using ReportCallback = std::function<void(const DeviceReportResponse&)>;

    /**
     * @brief 上报错误回调类型
     * @param error 错误信息
     */
    using ErrorCallback = std::function<void(const std::string&)>;

    /**
     * @brief 设备数据上报接口
     *
     * 向云端上报设备信息或心跳数据，用于更新设备的最后活动时间。
     * 与Android端API保持一致。
     *
     * 上报流程：
     * 1. 构建 JSON 请求体，包含设备认证信息和参数
     * 2. 自动添加 SDK 版本号到 params 中（格式：ai_2.1.0）
     * 3. 发送 HTTP POST 请求到设备上报接口
     * 4. 解析完整响应并调用用户回调
     *
     * 使用示例（业务层主动调用）：
     * @code
     * ReportClient client;
     * DeviceReportRequest request;
     * request.deviceId = "your_device_id";
     * request.deviceSecret = "your_device_secret";
     * request.productId = "your_product_id";
     * request.productKey = "your_product_key";
     * request.params["status"] = "online";
     * request.params["battery"] = std::string("85");
     * request.params["temperature"] = 25.5;
     *
     * client.dataReport(request,
     *     [](const DeviceReportResponse& response) {
     *         // 处理成功响应
     *         ESP_LOGI("app", "Report success: status=%d, message=%s",
     *                  response.status, response.message.c_str());
     *     },
     *     [](const std::string& error) {
     *         // 处理错误
     *         ESP_LOGE("app", "Report failed: %s", error.c_str());
     *     });
     * @endcode
     *
     * @param request 上报请求，包含设备认证信息和参数
     * @param onSuccess 成功回调，返回完整响应信息
     * @param onError 错误回调
     */
    void dataReport(
        const DeviceReportRequest& request,
        ReportCallback onSuccess,
        ErrorCallback onError
    );
};

} // namespace ai_sdk