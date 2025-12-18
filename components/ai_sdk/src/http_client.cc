#include "ai_sdk/http_client.h"
#include "esp_log.h"
#include <cstring>

namespace ai_sdk {

static const char* TAG = "HTTPClient";

esp_err_t HTTPClient::http_event_handler(esp_http_client_event_t *evt) {
    HTTPContext* ctx = static_cast<HTTPContext*>(evt->user_data);

    if (!ctx) {
        return ESP_FAIL;
    }

    switch(evt->event_id) {
        case HTTP_EVENT_ERROR:
            // HTTP连接错误
            ESP_LOGE(TAG, "HTTP_EVENT_ERROR");
            if (ctx->on_error) {
                ctx->on_error("HTTP_EVENT_ERROR");
            }
            delete ctx;
            return ESP_FAIL;

        case HTTP_EVENT_ON_CONNECTED:
            // HTTP连接成功
            ESP_LOGI(TAG, "HTTP_EVENT_ON_CONNECTED");
            break;

        case HTTP_EVENT_ON_HEADER:
            // 接收到HTTP响应头
            ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s",
                     evt->header_key, evt->header_value);
            break;

        case HTTP_EVENT_ON_DATA:
            // 接收到HTTP响应体数据
            // 将数据追加到response_data中，支持分块接收
            if (evt->data_len > 0) {
                ctx->response_data.append(static_cast<char*>(evt->data), evt->data_len);
            }
            break;

        case HTTP_EVENT_ON_FINISH:
            // HTTP请求完成
            // 调用成功回调并传递完整响应
            ESP_LOGI(TAG, "HTTP_EVENT_ON_FINISH");
            if (ctx->on_success) {
                ctx->on_success(ctx->response_data);
            }
            delete ctx;
            break;

        case HTTP_EVENT_DISCONNECTED:
            // HTTP连接断开
            ESP_LOGI(TAG, "HTTP_EVENT_DISCONNECTED");
            // 如果没有接收到任何数据，调用错误回调
            if (ctx->on_error && ctx->response_data.empty()) {
                ctx->on_error("HTTP_EVENT_DISCONNECTED");
            }
            delete ctx;
            break;

        case HTTP_EVENT_HEADERS_SENT:
            break;

        case HTTP_EVENT_REDIRECT:
            break;

        default:
            break;
    }

    return ESP_OK;
}

void HTTPClient::get(
    const std::string& url,
    const std::map<std::string, std::string>& headers,
    ResponseCallback onSuccess,
    ErrorCallback onError
) {
    // 创建HTTP请求上下文，用于传递回调和缓存响应
    HTTPContext* ctx = new HTTPContext();
    ctx->on_success = onSuccess;
    ctx->on_error = onError;
    ctx->client = this;

    // 配置HTTP客户端
    esp_http_client_config_t config = {};
    config.url = url.c_str();           // 设置请求URL
    config.event_handler = http_event_handler;  // 设置事件处理函数
    config.user_data = ctx;             // 传递上下文数据
    config.timeout_ms = 15000;          // 设置超时时间15秒

    // 初始化HTTP客户端
    esp_http_client_handle_t client = esp_http_client_init(&config);

    // 设置自定义请求头
    for (const auto& header : headers) {
        esp_http_client_set_header(client, header.first.c_str(), header.second.c_str());
    }

    // 设置请求方法为GET
    esp_http_client_set_method(client, HTTP_METHOD_GET);

    // 执行HTTP请求
    esp_err_t err = esp_http_client_perform(client);
    // 注：错误处理由事件处理器统一管理，避免双重释放

    // 清理HTTP客户端资源
    esp_http_client_cleanup(client);
}

void HTTPClient::post(
    const std::string& url,
    const std::string& body,
    const std::map<std::string, std::string>& headers,
    ResponseCallback onSuccess,
    ErrorCallback onError
) {
    // 创建HTTP请求上下文
    HTTPContext* ctx = new HTTPContext();
    ctx->on_success = onSuccess;
    ctx->on_error = onError;
    ctx->client = this;

    // 配置HTTP客户端
    esp_http_client_config_t config = {};
    config.url = url.c_str();           // 设置请求URL
    config.event_handler = http_event_handler;  // 设置事件处理函数
    config.user_data = ctx;             // 传递上下文数据
    config.timeout_ms = 15000;          // 设置超时时间15秒

    // 初始化HTTP客户端
    esp_http_client_handle_t client = esp_http_client_init(&config);

    // 设置自定义请求头
    for (const auto& header : headers) {
        esp_http_client_set_header(client, header.first.c_str(), header.second.c_str());
    }

    // 设置请求方法为POST
    esp_http_client_set_method(client, HTTP_METHOD_POST);
    // 设置POST请求体
    esp_http_client_set_post_field(client, body.c_str(), body.length());

    // 执行HTTP请求
    esp_err_t err = esp_http_client_perform(client);
    // 注：错误处理由事件处理器统一管理，避免双重释放

    // 清理HTTP客户端资源
    esp_http_client_cleanup(client);
}

} // namespace ai_sdk