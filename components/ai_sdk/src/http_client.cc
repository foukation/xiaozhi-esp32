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

    // ⚠️ ⚠️ ⚠️ 测试专用配置：禁用SSL证书验证 ⚠️ ⚠️ ⚠️
    //
    // 警告：以下配置仅供测试环境使用！生产环境必须启用证书验证！
    //
    // 问题背景：
    //   ESP-IDF的TLS层要求必须配置至少一种证书验证方式，否则会报错：
    //   "No server verification option set in esp_tls_cfg_t structure"
    //
    // 解决方案（测试环境禁用验证）：
    //   1. 设置 transport_type = HTTP_TRANSPORT_OVER_SSL（启用HTTPS）
    //   2. 设置 cert_pem = NULL（不加载CA证书）
    //   3. 设置 crt_bundle_attach = NULL（不加载系统CA证书包）
    //   4. 设置 skip_cert_common_name_check = true（跳过CN验证）
    //   5. 设置 use_global_ca_store = false（明确告知ESP-IDF不使用全局CA存储）
    //      ↑ 关键：此标志让ESP-IDF知道你已经配置了验证选项（虽然是禁用）
    //
    // ⚠️ 安全警告：
    //    不验证服务器证书会使系统易受中间人攻击！
    //    生产环境必须使用以下任一方式验证证书：
    //    - config.crt_bundle_attach = esp_crt_bundle_attach;   // 系统证书包（推荐）
    //    - config.cert_pem = ca_cert_pem;                      // 指定CA证书
    //    - config.pin_cert_sha256 = cert_hash;                 // 证书指纹
    config.transport_type = HTTP_TRANSPORT_OVER_SSL;
    config.crt_bundle_attach = NULL;              // 不加载系统CA证书包
    config.cert_pem = NULL;                       // 不加载CA证书（禁用验证）
    config.skip_cert_common_name_check = true;    // 跳过证书CN验证
    config.use_global_ca_store = false;           // 不使用全局CA存储（必须显式设置）

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

    // ⚠️ ⚠️ ⚠️ 测试专用配置：禁用SSL证书验证 ⚠️ ⚠️ ⚠️
    //
    // 警告：以下配置仅供测试环境使用！生产环境必须启用证书验证！
    //
    // 问题背景：
    //   ESP-IDF的TLS层要求必须配置至少一种证书验证方式，否则会报错：
    //   "No server verification option set in esp_tls_cfg_t structure"
    //
    // 解决方案（测试环境禁用验证）：
    //   1. 设置 transport_type = HTTP_TRANSPORT_OVER_SSL（启用HTTPS）
    //   2. 设置 cert_pem = NULL（不加载CA证书）
    //   3. 设置 crt_bundle_attach = NULL（不加载系统CA证书包）
    //   4. 设置 skip_cert_common_name_check = true（跳过CN验证）
    //   5. 设置 use_global_ca_store = false（明确告知ESP-IDF不使用全局CA存储）
    //      ↑ 关键：此标志让ESP-IDF知道你已经配置了验证选项（虽然是禁用）
    //
    // ⚠️ 安全警告：
    //    不验证服务器证书会使系统易受中间人攻击！
    //    生产环境必须使用以下任一方式验证证书：
    //    - config.crt_bundle_attach = esp_crt_bundle_attach;   // 系统证书包（推荐）
    //    - config.cert_pem = ca_cert_pem;                      // 指定CA证书
    //    - config.pin_cert_sha256 = cert_hash;                 // 证书指纹
    config.transport_type = HTTP_TRANSPORT_OVER_SSL;
    config.crt_bundle_attach = NULL;              // 不加载系统CA证书包
    config.cert_pem = NULL;                       // 不加载CA证书（禁用验证）
    config.skip_cert_common_name_check = true;    // 跳过证书CN验证
    config.use_global_ca_store = false;           // 不使用全局CA存储（必须显式设置）

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