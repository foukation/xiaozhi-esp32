#pragma once

#include <string>
#include <map>
#include <functional>
#include "esp_http_client.h"
#include "esp_err.h"

namespace ai_sdk {

// 前向声明
class HTTPClient;

/**
 * HTTP请求上下文结构
 */
struct HTTPContext {
    std::function<void(const std::string&)> on_success;  // 成功回调函数
    std::function<void(const std::string&)> on_error;       // 错误回调函数
    std::string response_data;                              // 响应数据缓存
    HTTPClient* client;                                      // HTTP客户端实例指针
};

/**
 * HTTP客户端封装类
 */
class HTTPClient {
public:
    using ResponseCallback = std::function<void(const std::string&)>;
    using ErrorCallback = std::function<void(const std::string&)>;

    /**
     * GET请求
     */
    void get(
        const std::string& url,
        const std::map<std::string, std::string>& headers,
        ResponseCallback onSuccess,
        ErrorCallback onError
    );

    /**
     * POST请求
     */
    void post(
        const std::string& url,
        const std::string& body,
        const std::map<std::string, std::string>& headers,
        ResponseCallback onSuccess,
        ErrorCallback onError
    );

private:
    static esp_err_t http_event_handler(esp_http_client_event_t *evt);
};

} // namespace ai_sdk