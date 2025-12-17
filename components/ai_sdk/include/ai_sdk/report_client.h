#pragma once

#include "ai_sdk/types.h"
#include <functional>
#include "esp_timer.h"

namespace ai_sdk {

/**
 * 上报客户端类
 */
class ReportClient {
public:
    using ReportCallback = std::function<void(const DeviceReportResponse&)>;
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

    ReportClient();
    ~ReportClient();

private:
    static void timerCallback(void* arg);
    void scheduleNextReport();

    std::function<void()> report_callback_;
    bool is_reporting_;
    esp_timer_handle_t timer_handle_;
};

} // namespace ai_sdk