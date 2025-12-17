#pragma once

#include <string>
#include <map>

namespace ai_sdk {

/**
 * 网关服务数据结构
 */
struct GatewayInfo {
    std::string token;          // 网关验证令牌
    std::string http_url;       // HTTP代理地址
    std::string ws_url;         // WebSocket代理地址
    int expires_in = 0;         // 代理有效期（单位：秒）
    int status = 0;             // 状态码（1：使用代理，0：不使用代理）
};

/**
 * 设备信息请求结构
 */
struct DeviceInfoRequest {
    std::string deviceNoType;   // 设备号类型
    std::string deviceNo;       // 设备编号
    std::string productId;      // 产品ID
    std::string productKey;     // 产品密钥
};

/**
 * 设备信息响应数据结构
 */
struct DeviceInfoData {
    std::string deviceId;       // 设备唯一标识
    std::string deviceSecret;   // 设备密钥
};

/**
 * 设备信息响应结构
 */
struct DeviceInfoResponse {
    int status = 0;             // 状态码
    DeviceInfoData data;        // 设备数据
};

/**
 * 设备报告请求数据结构
 */
struct DeviceReportRequest {
    std::map<std::string, std::string> params;  // 参数
    std::string eventType;                     // 事件类型
};

/**
 * 设备报告响应结构
 */
struct DeviceReportResponse {
    int status = 0;             // 状态码
};

} // namespace ai_sdk