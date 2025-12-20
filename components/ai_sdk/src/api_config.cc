#include "ai_sdk/api_config.h"

namespace ai_sdk {

// ===== 基础URL配置 =====
// 这些配置在编译时确定，提供云端服务的基础信息

/**
 * 终端智能服务平台基础URL
 * 中国移动物联网平台，提供设备管理和通信服务
 * 使用HTTPS协议，端口11443
 */
const char* ApiConfig::TERMINAL_INTELLIGENT_SERVICE_PLATFORM_BASE_URL =
    "https://ivs.chinamobiledevice.com:11443";

/**
 * 网关API路径
 * 用于获取代理服务器配置
 * 完整URL: {BASE_URL}/apgp/pl
 */
const char* ApiConfig::GATEWAY_API = "/apgp/pl";

/**
 * 设备信息获取API路径
 * 设备注册认证接口，通过设备号获取设备凭证
 * 完整URL: {BASE_URL}/v2/customer/device/secret/info
 */
const char* ApiConfig::OBTAIN_DEVICE_INFORMATION_API = "/v2/customer/device/secret/info";

/**
 * 设备数据上报API路径
 * 心跳/数据上报接口，报告设备状态和活跃信息
 * 完整URL: {BASE_URL}/v2/customer/device/report
 */
const char* ApiConfig::DEVICE_DATA_REPORT_API = "/v2/customer/device/report";

/**
 * HTTP请求超时时间（毫秒）
 * 设置为15秒，考虑网络环境和设备性能
 * 可根据实际需求调整：
 * - 降低：提高响应速度，但可能增加超时错误
 * - 提高：减少超时错误，但会增加等待时间
 */
const long ApiConfig::TIMEOUT = 15000L;

// ===== 运行时配置 =====
// 这些配置在程序运行时动态设置

/**
 * 是否使用代理（默认：false）
 * 由GatewayClient在启动时从云端获取代理配置后设置
 * true: 后续请求通过代理服务器转发
 * false: 直接连接云端服务器
 */
bool ApiConfig::useAgent = false;

/**
 * 代理服务器基础URL（默认：空）
 * 当useAgent为true时使用此地址
 * 格式: http(s)://proxy.example.com:port
 */
std::string ApiConfig::agentBaseUrl = "";

/**
 * API访问令牌（默认：空）
 * 用于代理服务器认证
 * 由GatewayClient获取并设置
 */
std::string ApiConfig::apiToken = "";

/**
 * 设备认证令牌（默认：空）
 * 设备注册后获得的认证信息
 * TODO: 目前未使用，需要实现设备签名逻辑
 * 计划用途：为每个API请求生成签名，防止伪造请求
 */
std::string ApiConfig::auth_token = "";

} // namespace ai_sdk