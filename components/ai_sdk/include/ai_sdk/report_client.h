#pragma once

#include "ai_sdk/types.h"
#include <functional>
#include "esp_timer.h"

namespace ai_sdk {

/**
 * 上报客户端类
 *
 * 用于设备数据上报和心跳管理，支持即时上报和定时上报两种模式。
 *
 * 功能特性：
 * - 即时上报：通过dataReport()接口立即上报设备数据
 * - 定时上报：通过startPeriodicReport()启动周期性心跳（12小时间隔）
 * - 随机偏移：定时上报会自动添加±15分钟的随机偏移，避免服务器压力
 * - ESP32定时器：使用硬件定时器，支持深度睡眠场景
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
     * ⼼跳接⼝/上报接⼝(定时向云端发送消息)
     * 设备（24小时至少上报一次）向云端上报信息，更新最后活动时间
     */
    void dataReport(
        const DeviceReportRequest& request,
        ReportCallback onSuccess,
        ErrorCallback onError
    );

    /**
     * 启动定时上报
     * 设备每隔12小时向平台上报设备数据信息
     */
    bool startPeriodicReport(std::function<void()> reportCallback);

    /**
     * 停止定时上报
     */
    void stopPeriodicReport();

    /**
     * 构造函数
     * 初始化上报客户端，创建时默认不启动定时上报
     */
    ReportClient();

    /**
     * 析构函数
     * 自动停止并清理定时上报资源
     */
    ~ReportClient();

private:
    /**
     * 定时器回调函数（静态）
     * @param arg 传递的参数，实际上是this指针
     */
    static void timerCallback(void* arg);

    /**
     * 调度下一次上报
     * 计算并启动下一次定时上报
     */
    void scheduleNextReport();

    /**
     * 用户提供的上报回调函数
     * 每次定时触发时调用
     */
    std::function<void()> report_callback_;

    /**
     * 定时上报状态标志
     * true: 正在运行，false: 已停止
     */
    bool is_reporting_;

    /**
     * ESP32定时器句柄
     * 用于管理硬件定时器资源
     */
    esp_timer_handle_t timer_handle_;
};

} // namespace ai_sdk