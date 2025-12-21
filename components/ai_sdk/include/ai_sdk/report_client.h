#pragma once

#include "ai_sdk/types.h"
#include <functional>

namespace ai_sdk {

/**
 * 上报客户端类
 *
 * 用于设备数据上报，提供即时上报功能。
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
 */
class ReportClient {
public:
    /**
     * 上报成功回调类型
     * @param response 上报响应
     */
    using ReportCallback = std::function<void(const DeviceReportResponse&)>;

    /**
     * 上报错误回调类型
     * @param error 错误信息
     */
    using ErrorCallback = std::function<void(const std::string&)>;

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
     * client.dataReport(request, onSuccess, onError);
     *
     * @param request 上报请求，包含自定义参数和事件类型
     * @param onSuccess 成功回调
     * @param onError 错误回调
     */
    void dataReport(
        const DeviceReportRequest& request,
        ReportCallback onSuccess,
        ErrorCallback onError
    );
};

} // namespace ai_sdk