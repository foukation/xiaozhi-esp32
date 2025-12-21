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
            // 内存管理：使用智能指针后，不再需要手动 delete ctx
            // ctx 的生命周期由 shared_ptr 自动管理
            ESP_LOGE(TAG, "HTTP_EVENT_ERROR");
            // 输出错误日志（与Android OkHttpManager保持一致）
            if (ctx->on_error) {
                ctx->on_error("HTTP_EVENT_ERROR");
            }
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
            // 内存管理：使用智能指针后，不再需要手动 delete ctx
            // ctx 的生命周期由 shared_ptr 自动管理
            ESP_LOGI(TAG, "HTTP_EVENT_ON_FINISH");
            // 输出响应日志（与Android OkHttpManager保持一致）
            ESP_LOGI(TAG, "response: %s", ctx->response_data.c_str());
            if (ctx->on_success) {
                ctx->on_success(ctx->response_data);
            }
            break;

        case HTTP_EVENT_DISCONNECTED:
            // HTTP连接断开
            // 内存管理：使用智能指针后，不再需要手动 delete ctx
            // ctx 的生命周期由 shared_ptr 自动管理
            ESP_LOGI(TAG, "HTTP_EVENT_DISCONNECTED");
            // 如果没有接收到任何数据，调用错误回调
            if (ctx->on_error && ctx->response_data.empty()) {
                ctx->on_error("HTTP_EVENT_DISCONNECTED");
            }
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
    // 输出HTTP请求日志（与Android OkHttpManager保持一致）
    ESP_LOGI(TAG, "request httpUrl: %s", url.c_str());
    ESP_LOGI(TAG, "request headers:");
    for (const auto& header : headers) {
        ESP_LOGI(TAG, "  %s: %s", header.first.c_str(), header.second.c_str());
    }

    // 创建HTTP请求上下文，使用智能指针自动管理生命周期
    // 内存管理优化：
    // - 使用 shared_ptr 避免手动 delete 导致的重复释放问题
    // - 即使在多个HTTP事件中被访问，shared_ptr 也能正确管理生命周期
    // - 当最后一个引用消失（函数返回 + HTTP客户端清理完成）时自动释放
    auto ctx = std::make_shared<HTTPContext>();
    ctx->on_success = onSuccess;
    ctx->on_error = onError;
    ctx->client = this;

    // 配置HTTP客户端
    esp_http_client_config_t config = {};
    config.url = url.c_str();           // 设置请求URL
    config.event_handler = http_event_handler;  // 设置事件处理函数
    config.timeout_ms = 15000;          // 设置超时时间15秒

    // 注意：user_data 存储裸指针，但实际生命周期由 shared_ptr 管理
    // 这种方式兼容ESP-IDF的C风格回调，同时享受C++智能指针的安全性
    config.user_data = ctx.get();

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
    // 输出HTTP请求日志（与Android OkHttpManager保持一致）
    ESP_LOGI(TAG, "request httpUrl: %s", url.c_str());
    ESP_LOGI(TAG, "request headers:");
    for (const auto& header : headers) {
        ESP_LOGI(TAG, "  %s: %s", header.first.c_str(), header.second.c_str());
    }
    ESP_LOGI(TAG, "request params: %s", body.c_str());

    // 创建HTTP请求上下文，使用智能指针自动管理生命周期
    // 内存管理优化：
    // - 使用 shared_ptr 避免手动 delete 导致的重复释放问题
    // - 即使在多个HTTP事件中被访问，shared_ptr 也能正确管理生命周期
    // - 当最后一个引用消失（函数返回 + HTTP客户端清理完成）时自动释放
    auto ctx = std::make_shared<HTTPContext>();
    ctx->on_success = onSuccess;
    ctx->on_error = onError;
    ctx->client = this;

    // 配置HTTP客户端
    esp_http_client_config_t config = {};
    config.url = url.c_str();           // 设置请求URL
    config.event_handler = http_event_handler;  // 设置事件处理函数
    config.timeout_ms = 15000;          // 设置超时时间15秒

    // 注意：user_data 存储裸指针，但实际生命周期由 shared_ptr 管理
    // 这种方式兼容ESP-IDF的C风格回调，同时享受C++智能指针的安全性
    config.user_data = ctx.get();

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