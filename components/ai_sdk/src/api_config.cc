#include "ai_sdk/api_config.h"

namespace ai_sdk {

// 基础URL配置
const char* ApiConfig::TERMINAL_INTELLIGENT_SERVICE_PLATFORM_BASE_URL =
    "https://ivs.chinamobiledevice.com:11443";

const char* ApiConfig::GATEWAY_API = "/apgp/pl";
const char* ApiConfig::OBTAIN_DEVICE_INFORMATION_API = "/v2/customer/device/secret/info";
const char* ApiConfig::DEVICE_DATA_REPORT_API = "/v2/customer/device/report";
const long ApiConfig::TIMEOUT = 15000L;

// 运行时配置
bool ApiConfig::useAgent = false;
std::string ApiConfig::agentBaseUrl = "";
std::string ApiConfig::apiToken = "";
std::string ApiConfig::auth_token = "";

} // namespace ai_sdk