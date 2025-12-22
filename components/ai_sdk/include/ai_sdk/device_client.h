#pragma once

#include "ai_sdk/types.h"
#include <functional>

namespace ai_sdk {

/**
 * @brief 设备客户端类
 * 用于管理与云端平台的设备注册和认证交互。
 * 与Android端API保持一致。
 */
class DeviceClient {
public:
    /**
     * @brief 设备信息获取成功回调类型
     * @param response 设备信息响应，包含完整的设备信息
     */
    using DeviceInfoSuccessCallback = std::function<void(const DeviceInfoResponse&)>;

    /**
     * @brief 设备信息获取错误回调类型
     * @param error 错误信息
     */
    using DeviceInfoErrorCallback = std::function<void(const std::string&)>;

    /**
     * @brief 构造函数
     */
    DeviceClient() = default;

    /**
     * @brief 析构函数
     */
    ~DeviceClient() = default;

    /**
     * @brief 获取设备信息
     * 设备通过产品信息及设备号从云端获取设备信息，前提是设备号已录入到云端平台。
     * 与Android端API保持一致。
     *
     * 此API用于设备首次注册认证，需要提供产品信息和设备标识，
     * 云端验证后会返回完整的设备信息，用于后续通信认证。
     *
     * 请求结构（与Android端一致）：
     * {
     *   "deviceNoType": "MAC|SN|IMEI",  // 设备号类型
     *   "deviceNo": "设备唯一序列号",      // 设备号，产品内唯一
     *   "productId": "产品ID",           // 平台创建产品时生成
     *   "productKey": "产品密钥"         // 平台创建产品时生成
     * }
     *
     * 响应结构（与Android端一致）：
     * {
     *   "status": 200,                   // 成功=200，其他值为异常
     *   "message": "响应消息",           // 错误提示信息
     *   "data": {
     *     "deviceId": "设备ID",         // 平台上唯一设备标识
     *     "deviceNo": "设备号",         // 产品内唯一标识设备的序列号
     *     "productId": "产品ID",        // 产品ID，平台创建产品时生成
     *     "deviceSecret": "设备密钥"    // 设备密钥，平台创建产品时生成
     *   }
     * }
     *
     * 使用示例：
     * @code
     * DeviceClient client;
     * DeviceInfoRequest request;
     * request.deviceNoType = "MAC";
     * request.deviceNo = "AA:BB:CC:DD:EE:FF";
     * request.productId = "your_product_id";
     * request.productKey = "your_product_key";
     *
     * client.obtainDeviceInformation(request,
     *     [](const DeviceInfoResponse& response) {
     *         // 处理成功响应
     *         if (response.status == 200) {
     *             ESP_LOGI("app", "Device registered successfully:");
     *             ESP_LOGI("app", "  deviceId: %s", response.data.deviceId.c_str());
     *             ESP_LOGI("app", "  deviceNo: %s", response.data.deviceNo.c_str());
     *         }
     *     },
     *     [](const std::string& error) {
     *         // 处理错误
     *         ESP_LOGE("app", "Device registration failed: %s", error.c_str());
     *     });
     * @endcode
     *
     * @param request 设备信息请求，包含设备类型、设备号、产品ID和产品密钥
     * @param onSuccess 成功回调，返回完整的设备信息
     * @param onError 错误回调，返回错误信息
     */
    void obtainDeviceInformation(
        const DeviceInfoRequest& request,
        DeviceInfoSuccessCallback onSuccess,
        DeviceInfoErrorCallback onError
    );

private:
    /**
     * HTTP响应处理回调
     * 解析云端返回的JSON数据，提取设备信息
     *
     * @param response HTTP响应字符串
     * @param onSuccess 设备信息解析成功后的回调
     * @param onError 设备信息解析失败后的回调
     */
    void onResponse(const std::string& response, DeviceInfoSuccessCallback onSuccess, DeviceInfoErrorCallback onError);

    /**
     * 错误处理回调
     * 处理设备信息获取过程中的错误
     *
     * @param error 错误信息
     * @param onError 用户提供的错误回调函数
     */
    void onError(const std::string& error, DeviceInfoErrorCallback onError);
};

} // namespace ai_sdk