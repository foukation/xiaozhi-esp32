#pragma once

#include <string>
#include <map>
#include <any>

namespace ai_sdk {

/**
 * @brief API响应状态码常量定义
 * 与Android端保持一致
 */
namespace ResCode {
    constexpr int SUC = 0;                    // 成功
    constexpr int AUTH_ERR = 401;             // 认证 token 错误
    constexpr int PARAM_ERR = 400;            // 参数错误
    constexpr int SERVE_ERR = 500;            // 服务内部服务器错误
    constexpr int IMAGE_ERR = 216101;         // 图片格式错误、图片未传递
}

/**
 * @brief 网关代理使用状态码
 * 与Android端保持一致
 */
namespace AgentUseCode {
    constexpr int USE = 1;                    // 使用代理
    constexpr int NOT = 0;                    // 不使用代理
}

/**
 * @brief 网关代理服务数据结构
 * 对应Android端的AgentServeData
 */
struct AgentServeData {
    std::string http;                         // HTTP代理地址
    std::string ws;                          // WebSocket代理地址
};

/**
 * @brief 网关服务响应结构
 * 对应Android端的AgentServeRes
 */
struct GatewayInfo {
    std::string token;                        // 网关验证令牌（可为空）
    AgentServeData data;                      // 代理服务数据
    int expires = 0;                          // 代理有效期（单位：秒）
    int status = 0;                           // 状态码（1：使用代理，0：不使用代理）
};

/**
 * @brief 设备数据上报响应数据结构
 * 对应Android端的DeviceReportData
 */
struct DeviceReportData {
    std::string deviceId;                     // 设备ID
    std::string protocolTypeTime;             // 协议类型时间
};

/**
 * @brief 设备数据上报响应结构
 * 对应Android端的DeviceReportResponse
 */
struct DeviceReportResponse {
    int status;                               // 响应状态码
    std::string message;                      // 响应消息
    DeviceReportData data;                    // 设备报告的具体数据对象
};

/**
 * @brief 设备信息请求结构
 * 对应Android端的DeviceInfoRequest
 */
struct DeviceInfoRequest {
    std::string deviceNoType;                 // 设备号类型：MAC、SN、IMEI
    std::string deviceNo;                     // 设备号，产品唯一标识设备的序列号
    std::string productId;                    // 产品ID，平台创建产品时生成
    std::string productKey;                   // 产品密钥，平台创建产品时生成
};

/**
 * @brief 设备信息响应数据结构
 * 对应Android端的DeviceData
 */
struct DeviceData {
    std::string deviceId;                     // 设备ID，平台上唯一设备标识
    std::string deviceNo;                     // 设备号，产品内唯一标识设备的序列号
    std::string productId;                    // 产品ID，平台创建产品时生成
    std::string deviceSecret;                 // 设备密钥，平台创建产品时生成
};

/**
 * @brief 设备信息响应结构
 * 对应Android端的DeviceInfoResponse
 */
struct DeviceInfoResponse {
    int code;                                 // 成功=200，其他值为异常（对应服务器返回的code字段）
    std::string message;                      // 响应结果，错误提示信息
    DeviceData data;                          // 响应数据
};

/**
 * @brief 设备数据上报请求结构
 * 对应Android端的DeviceReportRequest
 */
struct DeviceReportRequest {
    std::string deviceId;                     // 设备ID，用于唯一标识一个设备
    std::string deviceSecret;                 // 设备密钥，用于设备身份验证
    std::string productId;                    // 产品ID，标识设备所属的产品
    std::string productKey;                   // 产品密钥，用于产品身份验证
    std::map<std::string, std::any> params;   // 包含设备上报的额外参数
};

} // namespace ai_sdk