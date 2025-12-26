#pragma once

#include <string>
#include <cstdint>

namespace ai_sdk {

class DeviceClient;  // 前向声明

/**
 * @class AssistUtils
 * @brief 辅助工具类（对应 Android AssistUtils.kt）
 *
 * 提供URL参数构建、签名生成等工具函数。
 */
class AssistUtils {
public:
    /**
     * @brief 构建WebSocket URL参数
     * @param uri 基础URI（如 "wss://ivs.chinamobiledevice.com:11443/app-ws/v2/asr"）
     * @return 带参数的完整URL
     *
     * 构建的参数包括：
     * - sn: 会话唯一标识（UUID）
     * - deviceNo: 设备编号
     * - productKey: 产品密钥
     * - productId: 产品ID
     * - ts: 时间戳（毫秒）
     * - sign: MD5签名
     * - deviceId: 设备ID
     */
    static std::string wssParameter(const std::string& uri);

    /**
     * @brief 生成MD5签名
     * @param timestamp 时间戳（毫秒）
     * @return MD5签名字符串
     *
     * 使用deviceSecret和时间戳计算MD5签名。
     */
    static std::string signMd5(int64_t timestamp);

    /**
     * @brief 获取当前时间戳
     * @return 当前时间戳（毫秒）
     */
    static int64_t timestamp();

    /**
     * @brief 设置设备客户端（用于获取设备信息）
     * @param device_client 设备客户端实例
     */
    static void setDeviceClient(DeviceClient* device_client);

private:
    static DeviceClient* device_client_;  // 设备客户端（单例）
};

} // namespace ai_sdk
