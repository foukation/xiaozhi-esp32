#pragma once

#include <string>

namespace ai_sdk {

/**
 * API配置管理类
 */
class ApiConfig {
public:
    // 基础URL配置
    static const char* TERMINAL_INTELLIGENT_SERVICE_PLATFORM_BASE_URL;
    static const char* GATEWAY_API;
    static const char* OBTAIN_DEVICE_INFORMATION_API;
    static const char* DEVICE_DATA_REPORT_API;
    static const long TIMEOUT;

    // 运行时配置
    static bool useAgent;
    static std::string agentBaseUrl;
    static std::string apiToken;
    static std::string auth_token;
};

} // namespace ai_sdk