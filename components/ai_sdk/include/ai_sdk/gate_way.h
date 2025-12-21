#pragma once

#include "ai_sdk/types.h"
#include <functional>

namespace ai_sdk {

/**
 * 用于管理和处理与网关相关的操作和数据。
 * 网关作为不同设备和云端之间的桥梁，实现通信和数据传输。
 *
 * 参考 Android com.cmdc.ai.assist.api.GateWay
 */
class GateWay {
public:
    using DeviceInfoSuccessCallback = std::function<void(const DeviceInfoResponse&)>;
    using DeviceInfoErrorCallback = std::function<void(const std::string&)>;

    using GatewaySuccessCallback = std::function<void(const GatewayInfo&, const std::string&)>;
    using GatewayErrorCallback = std::function<void(const std::string&)>;

    using ReportSuccessCallback = std::function<void(const DeviceReportResponse&)>;
    using ReportErrorCallback = std::function<void(const std::string&)>;

    /**
     * 构造函数
     */
    GateWay() = default;

    /**
     * 析构函数
     */
    ~GateWay() = default;

    /**
     * 获取网关信息
     *
     * 使用回调函数处理成功和错误情况，使得该函数可以在异步操作中方便地处理结果
     *
     * @param onSuccess 成功回调，接收一个 AgentServeRes 类型的参数，表示网关信息
     * @param onError 错误回调，接收一个 String 类型的参数，表示错误信息
     */
    void getGateWay(
        GatewaySuccessCallback onSuccess,
        GatewayErrorCallback onError
    );

    /**
     * 获取设备信息
     * 设备通过产品信息及设备号从云端获取设备信息，前提是设备号已录入到云端平台；
     *
     * 使用回调函数来处理成功和错误的情况
     *
     * @param onSuccess 成功回调，当设备信息成功获取时调用，传递 DeviceInfoResponse 对象
     * @param onError 错误回调，当获取设备信息遇到错误时调用，传递错误信息字符串
     */
    void obtainDeviceInformation(
        DeviceInfoSuccessCallback onSuccess,
        DeviceInfoErrorCallback onError
    );

    /**
     * 心跳接口/上报接口(定时向云端发送消息)
     * 设备（24小时至少上报一次）向云端上报信息，更新最后活动时间。
     *
     * 请求策略(参考)
     * 设备每隔12小时向平台上报设备数据信息。
     * 实施步骤：
     * 1. 设备初始化：设备首次启动时，向平台上报数据并记录上报时间T_current。
     * 2. 计算下次上报时间：设备每次上报数据后，记录当前时间 T_current ，并计算下次上报时间 T_next ：
     * T_next = T_current + 12 小时 + 随机偏移量
     * 其中，随机偏移量可以是在-15分钟到+15分钟之间的一个随机值。
     * 3. 调度上报任务：设备根据计算出的 T_next 设置定时任务，确保在该时间点上报数据。
     * 注意：避免固定时间集中上报，造成服务器压力过大。
     *
     * 向设备发送报告请求它采用回调方式处理请求结果，
     * 分别定义了成功和错误处理的回调函数通过这种方式，调用者可以根据需要自定义如何处理成功响应和错误情况
     *
     * @param deviceReportRequest 包含设备报告请求的数据对象
     * @param onSuccess 当设备报告请求成功时调用的回调函数，传递一个 DeviceReportResponse 对象给它
     * @param onError 当设备报告请求失败时调用的回调函数，传递一个表示错误原因的字符串给它
     */
    void dataReport(
        const DeviceReportRequest& deviceReportRequest,
        ReportSuccessCallback onSuccess,
        ReportErrorCallback onError
    );

private:
    /**
     * HTTP响应处理回调 - 设备信息
     */
    void onDeviceInfoResponse(const std::string& response,
                              DeviceInfoSuccessCallback onSuccess,
                              DeviceInfoErrorCallback onError);

    /**
     * HTTP响应处理回调 - 网关信息
     */
    void onGatewayResponse(const std::string& response,
                           GatewaySuccessCallback onSuccess,
                           GatewayErrorCallback onError);

    /**
     * HTTP响应处理回调 - 上报
     */
    void onReportResponse(const std::string& response,
                          ReportSuccessCallback onSuccess,
                          ReportErrorCallback onError);

    /**
     * 错误处理回调
     */
    void onError(const std::string& error,
                 DeviceInfoErrorCallback onError);

    void onError(const std::string& error,
                 GatewayErrorCallback onError);

    void onError(const std::string& error,
                 ReportErrorCallback onError);
};

} // namespace ai_sdk
