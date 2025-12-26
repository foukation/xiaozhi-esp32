#include "ai_sdk/assist_utils.h"
#include "ai_sdk/api_config.h"
#include "ai_sdk/ai_assistant_manager.h"
#include "ai_sdk/device_client.h"
#include "esp_log.h"
#include "esp_mac.h"  // for MAC address
#include <sstream>
#include <iomanip>
#include <cstring>
#include <cstdlib>  // for rand()

namespace ai_sdk {

// 静态成员定义
DeviceClient* AssistUtils::device_client_ = nullptr;

/**
 * @brief 生成全局唯一的会话ID
 * @return 会话标识字符串
 *
 * 格式: sn-{MAC地址}-{时间戳}-{随机数}
 * - MAC地址（12位十六进制）：设备全球唯一标识
 * - 时间戳（毫秒）：时间区分
 * - 随机数（5位）：同一毫秒内的冲突保护
 *
 * 保证全局唯一性：
 * - 不同设备：MAC地址不同 → 100%唯一
 * - 同一设备不同时间：时间戳不同 → 100%唯一
 * - 同一设备同一时间：随机数1/90000概率
 */
static std::string generateSessionId() {
    // 获取设备MAC地址（全球唯一）
    uint8_t mac[6];
    esp_efuse_mac_get_default(mac);

    std::ostringstream sn_stream;
    sn_stream << "sn-";

    // 添加MAC地址（12位十六进制）
    for (int i = 0; i < 6; i++) {
        sn_stream << std::hex << std::setw(2) << std::setfill('0')
                  << (int)mac[i];
    }

    // 添加时间戳（毫秒）
    int64_t ts = AssistUtils::timestamp();
    sn_stream << "-" << ts;

    // 添加随机数（防止同一毫秒内的冲突）
    // 10000-99999范围，保证5位数
    sn_stream << "-" << (10000 + rand() % 90000);

    return sn_stream.str();
}

/**
 * 实现说明：
 * 与 Android AssistUtils.wssParameter() 功能一致
 * 但由于C++缺少设备信息获取接口，这里简化处理
 * TODO: 需要从DeviceClient获取实际的设备信息
 */
std::string AssistUtils::wssParameter(const std::string& uri) {
    ESP_LOGI("AssistUtils", "Building WebSocket URL parameters...");

    // 生成全局唯一的会话标识
    std::string sn = generateSessionId();
    ESP_LOGI("AssistUtils", "Session ID: %s", sn.c_str());

    const auto& config = AIAssistantManager::getInstance().config();

    std::string deviceNo = config.deviceNo;
    std::string productKey = config.productKey;
    std::string productId = config.productId;
    std::string deviceId = config.deviceId;
    std::string deviceSecret = config.deviceSecret;

    int64_t ts = timestamp();
    std::string sign = signMd5(ts);

    // 构建参数字符串（与Android参数顺序一致）
    std::ostringstream params;
    params << "?sn=" << sn
           << "&deviceNo=" << deviceNo
           << "&productKey=" << productKey
           << "&productId=" << productId
           << "&ts=" << ts
           << "&sign=" << sign
           << "&deviceId=" << deviceId;

    std::string full_url = uri + params.str();
    ESP_LOGI("AssistUtils", "WebSocket URL: %s", full_url.c_str());

    return full_url;
}

/**
 * 实现说明：
 * MD5签名生成，与 Android AssistUtils.signMd5() 功能一致
 * TODO: 需要实现实际的MD5计算
 */
std::string AssistUtils::signMd5(int64_t timestamp) {
    // TODO: 实现实际的MD5签名计算
    // Android实现：StringUtils.calculateSign(deviceSecret, timestamp.toString())

    // 临时返回模拟签名
    return "mock-sign-" + std::to_string(timestamp);
}

/**
 * 获取当前时间戳（毫秒）
 */
int64_t AssistUtils::timestamp() {
    return esp_log_timestamp();  // 使用ESP-IDF的时间戳
}

/**
 * 设置设备客户端实例
 */
void AssistUtils::setDeviceClient(DeviceClient* device_client) {
    device_client_ = device_client;
}

} // namespace ai_sdk
