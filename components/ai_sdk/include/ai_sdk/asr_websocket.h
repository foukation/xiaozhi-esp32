#pragma once

#include <string>
#include <functional>
#include "esp_websocket_client.h"

// FreeRTOS头文件
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

namespace ai_sdk {

/**
 * @struct AsrWebsocketConfig
 * @brief WebSocket客户端配置结构体
 *
 * 包含建立WebSocket连接所需的所有参数。
 * 支持SSL/TLS加密连接，可配置证书验证。
 */
struct AsrWebsocketConfig {
    /**
     * WebSocket服务器URL
     * 格式：wss://host:port/path 或 ws://host:port/path
     * 示例：wss://asr.example.com/v1/asr
     */
    std::string url;

    /**
     * 连接超时时间（毫秒）
     * 默认：10000ms（10秒）
     */
    int connect_timeout_ms = 10000;

    /**
     * 网络超时时间（毫秒）
     * 默认：30000ms（30秒）
     */
    int network_timeout_ms = 30000;

    /**
     * 心跳间隔（毫秒）
     * 0：禁用自动心跳
     * 默认：5000ms（5秒）
     */
    int ping_interval_ms = 5000;

    /**
     * SSL证书验证
     * true：验证服务器证书（生产环境）
     * false：跳过验证（测试环境）
     * 默认：true
     */
    bool verify_cert = true;

    /**
     * 缓冲区大小（字节）
     * 用于接收服务器响应
     * 默认：4096
     */
    int buffer_size = 4096;
};

/**
 * @class AsrWebsocket
 * @brief WebSocket客户端封装类
 *
 * 封装ESP-IDF WebSocket客户端，提供简洁的API。
 * 特性：
 * - 自动重连（指数退避算法）
 * - SSL/TLS加密
 * - 线程安全
 * - 事件驱动
 */
class AsrWebsocket {
public:
    /**
     * @brief 构造函数
     */
    AsrWebsocket();

    /**
     * @brief 析构函数
     * 自动关闭连接并释放资源
     */
    ~AsrWebsocket();

    /**
     * @typedef MessageCallback
     * @brief 消息接收回调类型
     * @param payload 消息内容
     * @param len 消息长度
     * @param type 消息类型（文本/二进制）
     */
    using MessageCallback = std::function<void(const uint8_t* payload, size_t len, int type)>;

    /**
     * @typedef EventCallback
     * @brief 连接事件回调类型
     * @param event 事件类型（连接/断开/错误）
     * @param message 事件描述
     */
    using EventCallback = std::function<void(int event, const std::string& message)>;

    /**
     * @brief 建立WebSocket连接
     * @param config 配置参数
     * @return bool 连接是否启动成功
     *
     * 启动连接流程：
     * 1. 创建WebSocket客户端
     * 2. 配置SSL参数（wss://）
     * 3. 启动连接任务
     * 4. 等待连接建立（异步）
     *
     * 注意：成功返回只表示开始连接，实际连接状态通过回调通知。
     */
    bool connect(const AsrWebsocketConfig& config);

    /**
     * @brief 发送文本消息
     * @param text 文本内容
     * @return bool 是否发送成功
     *
     * 用于发送JSON配置、控制指令等。
     * 线程安全，可以从任意任务调用。
     */
    bool sendText(const std::string& text);

    /**
     * @brief 发送二进制数据
     * @param data 数据缓冲区
     * @param len 数据长度
     * @return bool 是否发送成功
     *
     * 用于发送PCM音频数据。
     * 内部会自动分包发送（每包5120字节）。
     * 线程安全，可以从音频任务直接调用。
     */
    bool sendBinary(const uint8_t* data, size_t len);

    /**
     * @brief 关闭连接
     *
     * 发送关闭帧，断开连接，释放资源。
     * 调用后对象回到初始状态，可再次调用connect()。
     */
    void disconnect();

    /**
     * @brief 设置消息回调
     * @param callback 消息处理回调
     *
     * 回调函数将在WebSocket接收任务上下文中执行，
     * 应避免在回调中执行耗时操作。
     *
     * 消息类型：
     * - 0: 文本消息（JSON响应）
     * - 1: 二进制消息（不常用）
     */
    void setMessageCallback(MessageCallback callback);

    /**
     * @brief 设置事件回调
     * @param callback 事件处理回调
     *
     * 事件类型：
     * - WEBSOCKET_EVENT_CONNECTED: 连接成功
     * - WEBSOCKET_EVENT_DISCONNECTED: 连接断开
     * - WEBSOCKET_EVENT_ERROR: 网络错误
     * - WEBSOCKET_EVENT_DATA: 数据接收（兼容消息回调）
     */
    void setEventCallback(EventCallback callback);

    /**
     * @brief 检查连接状态
     * @return bool 是否已连接
     */
    bool isConnected() const;

    /**
     * @brief 检查是否正在连接
     * @return bool 是否正在连接中
     */
    bool isConnecting() const;

private:
    /**
     * @brief WebSocket客户端句柄
     * ESP-IDF原生WebSocket客户端实例
     */
    esp_websocket_client_handle_t client_ = nullptr;

    /**
     * @brief 连接状态标志
     * true: 连接已建立，可以正常通信
     * false: 连接断开或未建立
     */
    bool is_connected_ = false;

    /**
     * @brief 连接中标志
     * true: 正在连接中（已调用connect()，等待CONNECTED事件）
     * false: 空闲或已连接
     */
    bool is_connecting_ = false;

    /**
     * @brief 消息回调
     * 存储用户设置的消息处理回调
     */
    MessageCallback message_callback_;

    /**
     * @brief 事件回调
     * 存储用户设置的事件处理回调
     */
    EventCallback event_callback_;

    /**
     * @brief 连接互斥锁
     * 保护连接状态，确保线程安全
     */
    SemaphoreHandle_t connection_mutex_;

    /**
     * @brief 静态事件处理函数
     * @param event_handler_arg 用户参数（this指针）
     * @param event_base 事件基类
     * @param event_id 事件ID
     * @param event_data 事件数据
     *
     * 作为ESP-IDF事件循环的回调入口，
     * 内部转换为C++成员函数调用。
     */
    static void EventHandler(void* event_handler_arg,
                           esp_event_base_t event_base,
                           int32_t event_id,
                           void* event_data);
};

}  // namespace ai_sdk
