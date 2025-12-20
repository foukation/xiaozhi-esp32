#pragma once

#include <string>
#include <map>
#include <functional>
#include <memory>  // 添加智能指针支持
#include "esp_http_client.h"
#include "esp_err.h"

namespace ai_sdk {

// 前向声明
class HTTPClient;

/**
 * HTTP请求上下文结构
 *
 * 内存管理说明：
 * - 使用 shared_ptr 自动管理生命周期，避免手动 delete 导致的重复释放问题
 * - 在 HTTP 事件处理器中不再需要手动删除 ctx 对象
 * - shared_ptr 会在最后一个引用消失时自动释放内存
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
    /**
     * HTTP事件处理器（静态）
     *
     * 处理ESP-IDF HTTP客户端的所有事件，包括：
     * - 连接建立/断开
     * - 响应头接收
     * - 响应体数据接收
     * - 请求完成
     * - 错误处理
     *
     * 工作原理：
     * - ESP-IDF的事件回调必须是静态函数
     * - 通过evt->user_data获取HTTPContext指针
     * - 根据事件类型调用相应的回调函数（on_success/on_error）
     *
     * 内存管理：使用智能指针管理HTTPContext生命周期，避免手动释放
     *
     * @param evt HTTP事件结构体，包含事件类型和相关数据
     * @return ESP_OK表示成功，ESP_FAIL表示失败
     */
    static esp_err_t http_event_handler(esp_http_client_event_t *evt);
};

} // namespace ai_sdk