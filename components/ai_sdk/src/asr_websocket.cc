/**
 * @file asr_websocket.cc
 * @brief WebSocket客户端实现
 *
 * 本文件是AsrWebsocket类的实现框架，包含类成员定义和接口实现，
 * 具体需要补充WebSocket连接管理、事件处理、重连逻辑等。
 */

#include "ai_sdk/asr_websocket.h"
#include "esp_log.h"

// FreeRTOS头文件
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

namespace ai_sdk {

static const char* TAG = "AsrWebsocket";

AsrWebsocket::AsrWebsocket() {
    // 创建互斥锁，用于保护连接状态和回调函数的线程安全访问
    // 二进制信号量，初始化为1（解锁状态）
    connection_mutex_ = xSemaphoreCreateBinary();
    if (connection_mutex_) {
        xSemaphoreGive(connection_mutex_);  // 初始化为解锁状态
    } else {
        ESP_LOGE(TAG, "Failed to create connection mutex");
    }

    // 初始化连接状态为断开
    is_connected_ = false;
    is_connecting_ = false;
}

AsrWebsocket::~AsrWebsocket() {
    // 析构函数中确保连接被关闭
    // 防止资源泄漏
    if (client_) {
        disconnect();
    }

    // 删除互斥锁，释放系统资源
    if (connection_mutex_) {
        vSemaphoreDelete(connection_mutex_);
        connection_mutex_ = nullptr;
    }
}

bool AsrWebsocket::connect(const AsrWebsocketConfig& config) {
    // 连接建立是异步操作，需要保证线程安全
    // 使用互斥锁防止多个任务同时尝试连接
    if (xSemaphoreTake(connection_mutex_, pdMS_TO_TICKS(1000)) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to acquire mutex for connection");
        return false;
    }

    // 检查是否已经在连接或已连接状态
    // 如果已连接，需要先断开旧连接才能建立新连接
    if (is_connected_ || is_connecting_) {
        ESP_LOGW(TAG, "Already connected or connecting, disconnect first");
        xSemaphoreGive(connection_mutex_);
        return false;
    }

    // 验证URL格式是否合法
    // WebSocket URL必须以ws://或wss://开头
    if (config.url.find("ws://") != 0 && config.url.find("wss://") != 0) {
        ESP_LOGE(TAG, "Invalid WebSocket URL format: %s", config.url.c_str());
        ESP_LOGE(TAG, "URL must start with ws:// or wss://");
        xSemaphoreGive(connection_mutex_);
        return false;
    }

    // 创建WebSocket客户端配置
    // 配置参数来源于用户传入的AsrWebsocketConfig结构体
    // 注意：初始化顺序必须与结构体声明顺序一致
    esp_websocket_client_config_t ws_config = {
        .uri = config.url.c_str(),                           // WebSocket服务器URL
        .buffer_size = config.buffer_size,                   // 接收缓冲区大小
        .reconnect_timeout_ms = config.connect_timeout_ms,   // 重连超时（毫秒）
        .network_timeout_ms = config.network_timeout_ms,     // 网络超时（毫秒）
        .ping_interval_sec = (size_t)(config.ping_interval_ms / 1000),  // 心跳间隔（秒），显式转换为size_t
    };

    // 创建WebSocket客户端实例
    // 内部会分配内存和资源，失败时返回nullptr
    // ESP-IDF v5.4: 使用 esp_websocket_client_init 而不是 esp_websocket_client_create
    client_ = esp_websocket_client_init(&ws_config);
    if (!client_) {
        ESP_LOGE(TAG, "Failed to create WebSocket client instance");
        xSemaphoreGive(connection_mutex_);
        return false;
    }

    // 注册事件回调函数
    // 所有WebSocket事件（连接、断开、数据、错误）都会触发EventHandler
    // this指针作为用户参数传递给事件处理函数
    esp_websocket_register_events(client_, WEBSOCKET_EVENT_ANY, EventHandler, this);

    // 启动WebSocket客户端
    // 异步操作，立即返回
    // 实际连接结果通过事件回调通知
    esp_err_t ret = esp_websocket_client_start(client_);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start WebSocket client: %s", esp_err_to_name(ret));
        // 启动失败，清理资源
        esp_websocket_client_destroy(client_);
        client_ = nullptr;
        xSemaphoreGive(connection_mutex_);
        return false;
    }

    // 标记为连接中状态
    // 在此状态下可以继续调用sendText/sendBinary，数据会被缓存
    is_connecting_ = true;
    ESP_LOGI(TAG, "WebSocket client started, connecting to %s", config.url.c_str());

    xSemaphoreGive(connection_mutex_);
    return true;  // 成功启动连接流程（异步）
}

void AsrWebsocket::disconnect() {
    // TODO: 实现断开连接
    // if (!client_) {
    //     return;
    // }
    //
    // esp_websocket_client_close(client_, pdMS_TO_TICKS(1000));
    // esp_websocket_client_destroy(client_);
    // client_ = nullptr;
    // is_connected_ = false;
    // is_connecting_ = false;
}

bool AsrWebsocket::sendText(const std::string& text) {
    // TODO: 实现发送文本
    // if (!client_ || !is_connected_) {
    //     ESP_LOGE(TAG, "Not connected");
    //     return false;
    // }
    //
    // esp_err_t ret = esp_websocket_client_send_text(
    //     client_,
    //     text.c_str(),
    //     text.length(),
    //     pdMS_TO_TICKS(1000)
    // );
    //
    // return ret == ESP_OK;
    return false;  // TODO: 完成实现
}

bool AsrWebsocket::sendBinary(const uint8_t* data, size_t len) {
    // TODO: 实现发送二进制数据
    // if (!client_ || !is_connected_) {
    //     ESP_LOGE(TAG, "Not connected");
    //     return false;
    // }
    //
    // esp_err_t ret = esp_websocket_client_send_bin(
    //     client_,
    //     data,
    //     len,
    //     pdMS_TO_TICKS(1000)
    // );
    //
    // return ret == ESP_OK;
    return false;  // TODO: 完成实现
}

void AsrWebsocket::setMessageCallback(MessageCallback callback) {
    // 设置消息回调函数
    // 当 WebSocket 收到服务器消息时，会通过此回调通知上层
    // 回调参数: data(消息数据), len(数据长度), type(消息类型: 0=文本, 1=二进制)
    message_callback_ = callback;
}

void AsrWebsocket::setEventCallback(EventCallback callback) {
    // 设置事件回调函数
    // 当 WebSocket 连接状态发生变化时（连接成功、断开、错误等），会通过此回调通知上层
    // 回调参数: event_id(事件类型), message(事件描述)
    event_callback_ = callback;
}

bool AsrWebsocket::isConnected() const {
    // 返回当前连接状态
    // true: 已建立连接，可以收发数据
    // false: 未连接或连接中
    return is_connected_;
}

bool AsrWebsocket::isConnecting() const {
    // 返回连接中状态
    // true: 正在连接（已调用start，但尚未收到connected事件）
    // false: 空闲或已连接
    return is_connecting_;
}

// WebSocket事件处理器
void AsrWebsocket::EventHandler(void* event_handler_arg,
                                esp_event_base_t event_base,
                                int32_t event_id,
                                void* event_data) {
    // event_handler_arg 是 AsrWebsocket 实例指针
    AsrWebsocket* websocket = static_cast<AsrWebsocket*>(event_handler_arg);
    if (!websocket) {
        ESP_LOGE(TAG, "Invalid event handler argument");
        return;
    }

    // 处理 WebSocket 事件
    switch (event_id) {
        case WEBSOCKET_EVENT_CONNECTED:
            {
                ESP_LOGI(TAG, "WebSocket connected");
                websocket->is_connected_ = true;
                websocket->is_connecting_ = false;

                // 调用事件回调
                if (websocket->event_callback_) {
                    websocket->event_callback_(event_id, "Connected");
                }

                // 调用消息回调（通知上层）
                if (websocket->message_callback_) {
                    const char* msg = "CONNECTED";
                    websocket->message_callback_(
                        reinterpret_cast<const uint8_t*>(msg),
                        strlen(msg),
                        0  // 0表示文本消息
                    );
                }
            }
            break;

        case WEBSOCKET_EVENT_DISCONNECTED:
            {
                ESP_LOGW(TAG, "WebSocket disconnected");
                websocket->is_connected_ = false;
                websocket->is_connecting_ = false;

                if (websocket->event_callback_) {
                    websocket->event_callback_(event_id, "Disconnected");
                }
            }
            break;

        case WEBSOCKET_EVENT_DATA:
            {
                auto* data = (esp_websocket_event_data_t*)event_data;
                if (data && websocket->message_callback_) {
                    websocket->message_callback_(
                        (const uint8_t*)data->data_ptr,
                        data->data_len,
                        data->op_code  // 使用实际的WebSocket操作码（1=文本，2=二进制）
                    );
                }
            }
            break;

        case WEBSOCKET_EVENT_ERROR:
            {
                ESP_LOGE(TAG, "WebSocket error");
                websocket->is_connected_ = false;
                websocket->is_connecting_ = false;

                if (websocket->event_callback_) {
                    websocket->event_callback_(event_id, "Error");
                }
            }
            break;

        default:
            ESP_LOGD(TAG, "Unhandled WebSocket event: %ld", event_id);
            break;
    }
}

}  // namespace ai_sdk
