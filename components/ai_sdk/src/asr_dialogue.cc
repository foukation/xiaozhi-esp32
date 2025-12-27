/**
 * @file asr_dialogue.cc
 * @brief ASR智能对话实现文件
 *
 * 本文件包含AsrDialogue类的实现框架，定义了基本的类结构和接口，
 * 具体实现需要补充WebSocket通信、JSON解析、状态管理等逻辑。
 */

#include "ai_sdk/asr_dialogue.h"
#include "ai_sdk/asr_websocket.h"
#include "ai_sdk/api_config.h"
#include "ai_sdk/assist_utils.h"
#include "ai_sdk/ai_assistant_manager.h"
#include "esp_log.h"
#include "esp_websocket_client.h"
#include "cJSON.h"
#include "esp_timer.h"
#include <mutex>

// FreeRTOS头文件
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace ai_sdk {

// 日志标签
static const char* TAG = "AsrDialogue";

/**
 * @class AsrDialogue::Impl
 * @brief 私有实现类（PIMPL模式）
 *
 * 隐藏实现细节，减少头文件依赖，提高编译速度，
 * 同时提供更好的二进制兼容性。
 */
class AsrDialogue::Impl {
private:
    static constexpr int CONNECTION_TIMEOUT_MS = 30000;  // 连接超时时间（30秒）
    static constexpr size_t RECEIVE_TASK_STACK = 4096;   // 接收任务堆栈
    static constexpr UBaseType_t RECEIVE_TASK_PRIO = 5;  // 接收任务优先级

public:
    /**
     * @brief 启动ASR识别
     * @return bool 是否启动成功
     *
     * 实现要点（与Android一致）：
     * 1. 检查当前状态（是否已在识别中）
     * 2. 构建WebSocket URL（内部构建，不是外部传入）
     *   - 基础URL: ApiConfig::WSS_WEBSOCKET_ASR_BASE_URL + ApiConfig::ASR_INTELLIGENT_DIALOGUE_API
     *   - 调用 AssistUtils::wssParameter() 添加参数和签名
     * 3. 创建WebSocket配置（AsrWebsocketConfig，设置超时时间）
     * 4. 建立WebSocket连接（异步）
     * 5. 等待连接成功（使用信号量，30秒超时）
     * 6. 发送ASR配置参数（sample rate, format等）
     * 7. 标记为识别中状态
     *
     * 错误处理：
     * - URL构建失败：返回false，通过error_callback_通知
     * - 网络连接失败：异步通知，尝试重连
     * - 连接超时：返回false
     * - 配置发送失败：记录日志，继续尝试
     *
     * 线程安全：使用state_mutex_保护状态变量。
     */
    bool start() {
        // ========================================
        // 第1步：加锁检查状态
        // ========================================
        // 注意：这里使用代码块 {} 限制锁的作用范围
        // 锁只在检查状态时持有，检查完毕后立即释放
        // 这样可以避免后续等待信号量时发生死锁
        {
            std::lock_guard<std::mutex> lock(*state_mutex_);
            if (is_recognizing_) {
                ESP_LOGW(TAG, "Already recognizing, cannot start again");
                return false;
            }
        }  // ← 锁在这里自动释放

        ESP_LOGI(TAG, "Starting ASR recognition...");

        // ========================================
        // 第2步：构建完整WebSocket URL（与Android一致）
        // ========================================
        // 基础URL: wss://ivs.chinamobiledevice.com:11443/app-ws/v2/asr
        std::string base_url = std::string(ApiConfig::WSS_WEBSOCKET_ASR_BASE_URL) +
                              ApiConfig::ASR_INTELLIGENT_DIALOGUE_API;
        ESP_LOGI(TAG, "Base URL: %s", base_url.c_str());

        // 添加参数和签名（sn, deviceNo, productKey, ts, sign等）
        std::string ws_url = AssistUtils::wssParameter(base_url);
        if (ws_url.empty()) {
            ESP_LOGE(TAG, "Failed to build WebSocket URL");
            if (error_callback_) {
                error_callback_(-1, "Failed to build WebSocket URL");
            }
            return false;
        }
        ESP_LOGI(TAG, "Full URL: %s", ws_url.c_str());

        // ========================================
        // 第3步：配置WebSocket参数（与Android超时配置一致）
        // ========================================
        // - 连接超时：10秒
        // - 网络超时：30秒
        // - 心跳间隔：30秒
        AsrWebsocketConfig config;
        config.url = ws_url;
        config.connect_timeout_ms = 10000;   // 10秒
        config.network_timeout_ms = 30000;   // 30秒
        config.ping_interval_ms = 30000;     // 30秒（心跳）
        config.buffer_size = 4096;           // 4KB缓冲区
        ESP_LOGI(TAG, "WebSocket config created: timeout=%d/%d/%d ms",
                 config.connect_timeout_ms,
                 config.network_timeout_ms,
                 config.ping_interval_ms);

        // ========================================
        // 第4步：重置信号量（确保在获取前为0）
        // ========================================
        if (connection_semaphore_ != nullptr) {
            xSemaphoreTake(connection_semaphore_, 0);  // 清空信号量
        }

        // ========================================
        // 第5步：注册事件回调（处理连接成功、断开、错误事件）
        // ========================================
        // 通过 event_callback_ 接收底层 WebSocket 状态变化通知
        // 注意：回调函数在 WebSocket 事件线程中执行，不是 start() 所在的线程
        websocket_.setEventCallback([this](int event_id, const std::string& message) {
            // event_id 对应 esp_websocket_client 的事件枚举值
            // 使用枚举值而不是硬编码数字，确保与 ESP-IDF 版本兼容
            ESP_LOGI(TAG, "Event: WebSocket event_id=%d", event_id);
            switch (event_id) {
                case WEBSOCKET_EVENT_CONNECTED:
                    ESP_LOGI(TAG, "Event: WebSocket connected");
                    this->onConnected();
                    break;

                case WEBSOCKET_EVENT_DISCONNECTED:
                    ESP_LOGW(TAG, "Event: WebSocket disconnected - %s", message.c_str());
                    this->onDisconnected(message);
                    // 释放信号量，让 start() 立即返回失败（如果还在等待连接）
                    if (connection_semaphore_ != nullptr) {
                        xSemaphoreGive(connection_semaphore_);
                    }
                    break;

                case WEBSOCKET_EVENT_ERROR:
                    ESP_LOGE(TAG, "Event: WebSocket error - %s", message.c_str());
                    // 释放信号量，让 start() 立即返回失败
                    if (connection_semaphore_ != nullptr) {
                        xSemaphoreGive(connection_semaphore_);
                    }
                    // 通知上层发生错误
                    if (error_callback_) {
                        error_callback_(-1, "WebSocket error: " + message);
                    }
                    break;

                default:
                    ESP_LOGD(TAG, "Event: Unknown event_id=%d, message=%s", event_id, message.c_str());
                    break;
            }
        });

        // ========================================
        // 第6步：注册消息回调（处理服务器发送的数据）
        // ========================================
        // 用于接收 ASR 识别结果、对话响应等业务消息
        websocket_.setMessageCallback([this](const uint8_t* data, size_t len, int type) {
            // type: 1=文本消息, 2=二进制消息（WebSocket opcode）
            if (type == 1) {
                // 文本消息：JSON 格式的 ASR 结果或对话响应
                ESP_LOGI(TAG, "Received text message, len=%d", len);
                // 调用 parseMessage() 解析并分发到对应回调
                this->parseMessage(data, len);
            } else if (type == 2) {
                // 二进制消息：可能是音频数据等
                ESP_LOGD(TAG, "Received binary message, len=%d", len);
            }
        });

        // ========================================
        // 第7步：发起 WebSocket 连接（异步）
        // ========================================
        if (!websocket_.connect(config)) {
            ESP_LOGE(TAG, "Failed to initiate WebSocket connection");
            if (error_callback_) {
                error_callback_(-1, "Failed to initiate WebSocket connection");
            }
            return false;
        }

        // ========================================
        // 第8步：等待连接成功
        // ========================================
        // 模拟 Android CountDownLatch.await()
        // 等待事件：onConnected() 在连接成功时释放信号量
        // 如果连接失败或出错，event_callback_ 也会释放信号量，让等待立即返回
        //
        // 重要：此时没有持有 state_mutex_ 锁！
        // 这样 onConnected() 可以获取锁并释放信号量，不会发生死锁
        ESP_LOGI(TAG, "Waiting for connection... (timeout=%d ms)", CONNECTION_TIMEOUT_MS);
        if (connection_semaphore_ != nullptr) {
            // 等待连接成功信号，最多30秒
            if (xSemaphoreTake(connection_semaphore_, pdMS_TO_TICKS(CONNECTION_TIMEOUT_MS)) != pdTRUE) {
                ESP_LOGE(TAG, "Connection timeout after %d ms", CONNECTION_TIMEOUT_MS);
                websocket_.disconnect();
                if (error_callback_) {
                    error_callback_(-1, "Connection timeout");
                }
                return false;
            }
        }

        // ========================================
        // 第9步：检查连接结果并更新状态
        // ========================================
        // 重新加锁，检查连接状态并设置 is_recognizing_
        {
            std::lock_guard<std::mutex> lock(*state_mutex_);

            // 检查是否真的连接成功
            if (!is_connected_) {
                ESP_LOGE(TAG, "Connection failed");
                websocket_.disconnect();
                if (error_callback_) {
                    error_callback_(-1, "Connection failed");
                }
                return false;
            }

            // 标记为识别中状态
            is_recognizing_ = true;
        }  // ← 锁在这里自动释放

        ESP_LOGI(TAG, "WebSocket connected successfully!");

        // ========================================
        // 第10步：调用连接成功回调（对应 Android onConnected()）
        // ========================================
        if (connected_callback_) {
            connected_callback_();
        }

        // ========================================
        // 第11步：发送 Start Signal（与 Android 一致）
        // ========================================
        // 发送完整的 Start Signal，包含设备信息和配置参数
        // 参考 Android DealSotaOne.createStartSignal()
        sendStartSignal();

        ESP_LOGI(TAG, "ASR recognition started successfully");

        // TODO: 第12步：启动接收任务（处理服务器响应）
        // xTaskCreate(ReceiveTaskEntry, "asr_receive", RECEIVE_TASK_STACK, this, ...);

        return true;
    }

    /**
     * @brief WebSocket连接成功回调
     *
     * 由 event_callback_ 在收到 WEBSOCKET_EVENT_CONNECTED 事件时调用
     * 负责更新连接状态并释放信号量，唤醒等待中的 start() 函数
     *
     * 执行顺序：
     * 1. 先加锁更新 is_connected_ 状态
     * 2. 释放锁
     * 3. 再释放信号量唤醒 start()
     *
     * 这样可以确保 start() 被唤醒后检查 is_connected_ 时，状态已经是 true
     */
    void onConnected() {
        ESP_LOGI(TAG, "onConnected: WebSocket connected, updating state...");

        // 先加锁更新状态
        {
            std::lock_guard<std::mutex> lock(*state_mutex_);
            is_connected_ = true;
            ESP_LOGI(TAG, "onConnected: is_connected_ = true");
        }  // ← 锁在这里释放

        // 释放信号量，唤醒等待的 start() 函数
        // 注意：必须在锁释放之后再释放信号量
        // 这样 start() 被唤醒后可以立即获取锁检查状态
        if (connection_semaphore_ != nullptr) {
            ESP_LOGI(TAG, "onConnected: Releasing semaphore to wake up start()");
            xSemaphoreGive(connection_semaphore_);
        }
    }

    /**
     * @brief WebSocket断开回调
     * @param error 错误信息
     */
    void onDisconnected(const std::string& error = "") {
        ESP_LOGW(TAG, "WebSocket disconnected: %s", error.c_str());

        std::lock_guard<std::mutex> lock(*state_mutex_);
        is_connected_ = false;
    }

    /**
     * @brief 连接同步信号量
     * 用于等待WebSocket连接建立（模仿 Android CountDownLatch）
     * - 连接成功：give() 释放信号量
     * - 连接失败：超时或错误处理
     *
     * 实现方式：使用 FreeRTOS 二值信号量（初始为0，连接成功后give）
     */
    SemaphoreHandle_t connection_semaphore_ = nullptr;

    /**
     * @brief 构造函数
     *
     * 初始化内部状态，创建必要的资源（互斥锁、信号量等）。
     */
    Impl() {
        // 创建状态互斥锁（保护 is_recognizing_ 和 is_connected_）
        state_mutex_ = std::make_unique<std::mutex>();

        // 创建回调互斥锁（保护回调函数指针）
        callback_mutex_ = std::make_unique<std::mutex>();

        // 创建连接信号量（初始为0，用于连接同步）
        connection_semaphore_ = xSemaphoreCreateBinary();
        if (connection_semaphore_ == nullptr) {
            ESP_LOGE(TAG, "Failed to create connection semaphore");
        }

        is_recognizing_ = false;
        is_connected_ = false;
        receive_task_ = nullptr;

        ESP_LOGI(TAG, "AsrDialogue Impl initialized successfully");
    }

    /**
     * @brief 析构函数
     *
     * 释放所有资源，关闭WebSocket连接，销毁互斥锁和信号量。
     */
    ~Impl() {
        if (is_recognizing_) {
            stop();
        }

        // 等待接收任务结束
        if (receive_task_ != nullptr) {
            vTaskDelete(receive_task_);
            receive_task_ = nullptr;
        }

        // 删除连接信号量
        if (connection_semaphore_ != nullptr) {
            vSemaphoreDelete(connection_semaphore_);
            connection_semaphore_ = nullptr;
        }
    }

    /**
     * @brief 停止ASR识别
     *
     * 实现要点：
     * 1. 检查当前状态（是否正在识别）
     * 2. 发送结束信号（{"type": "finish"}）
     * 3. 等待服务器确认（超时2秒）
     * 4. 关闭WebSocket连接
     * 5. 停止接收任务
     * 6. 重置所有状态变量
     *
     * 注意：停止操作可能需要100-500ms，应考虑异步实现。
     */
    void stop() {
        ESP_LOGI(TAG, "Stopping ASR recognition...");

        // 检查当前状态
        {
            std::lock_guard<std::mutex> lock(*state_mutex_);
            if (!is_recognizing_) {
                ESP_LOGW(TAG, "Not recognizing, nothing to stop");
                return;
            }
        }

        // 发送 Finish Signal（与 Android 一致）
        // 通知服务器停止识别
        std::string finish_msg = R"({"type": "finish"})";
        ESP_LOGI(TAG, "Sending Finish Signal: %s", finish_msg.c_str());

        if (websocket_.sendText(finish_msg)) {
            ESP_LOGI(TAG, "Finish Signal sent successfully");

            // 等待服务器响应（最多 2 秒）
            // 给服务器时间处理并返回最终结果
            ESP_LOGI(TAG, "Waiting for server response (2 seconds)...");
            vTaskDelay(pdMS_TO_TICKS(2000));
        } else {
            ESP_LOGW(TAG, "Failed to send Finish Signal");
        }

        // 断开 WebSocket 连接
        ESP_LOGI(TAG, "Disconnecting WebSocket...");
        websocket_.disconnect();

        // 重置状态变量
        {
            std::lock_guard<std::mutex> lock(*state_mutex_);
            is_recognizing_ = false;
            is_connected_ = false;
        }

        // 调用完成回调
        if (complete_callback_) {
            ESP_LOGI(TAG, "Calling complete callback");
            complete_callback_();
        }

        ESP_LOGI(TAG, "ASR recognition stopped successfully");
    }

    /**
     * @brief 发送音频数据
     * @param data PCM音频数据
     * @param len 数据长度
     *
     * 实现要点：
     * 1. 检查连接状态（必须已连接且正在识别）
     * 2. 数据缓存（如果网络慢）
     * 3. 分包发送（每包5120字节）
     * 4. 添加音频帧头（时间戳、序列号）
     * 5. 处理发送失败情况
     *
     * 性能优化：
     * - 使用内存池减少malloc/free
     * - 批量发送减少系统调用
     * - 双缓冲机制避免阻塞
     *
     * 错误处理：
     * - 连接断开：调用error_callback_
     * - 发送失败：重试3次，失败后停止识别
     */
    void sendAudio(const uint8_t* data, size_t len) {
        // TODO: 实现音频发送
        // if (!is_recognizing_ || !is_connected_) {
        //     ESP_LOGW(TAG, "Not ready, dropping audio data");
        //     return;
        // }
        //
        // TODO: 分包处理（如果data > 5120）
        // const size_t CHUNK_SIZE = 5120;
        // size_t offset = 0;
        // while (offset < len) {
        //     size_t chunk_len = std::min(CHUNK_SIZE, len - offset);
        //     websocket_.sendBinary(data + offset, chunk_len);
        //     offset += chunk_len;
        // }
    }

    /**
     * @brief 设置回调函数
     * @param connected_cb 连接成功回调（可选，可为nullptr）
     * @param asr_cb ASR识别结果回调（中间结果和最终结果）
     * @param dialogue_cb 对话结果回调
     * @param error_cb 错误回调
     * @param complete_cb 识别完成回调（可选，可为nullptr）
     *
     * 实现要点：
     * 1. 线程安全：使用互斥锁保护回调函数指针
     * 2. 空值检查：允许传入nullptr，表示忽略该类型回调
     * 3. 立即生效：设置后立即用于后续事件
     * 4. API兼容：与 Android ASRIntelligentDialogue.setListener() 保持一致
     *
     * 回调调用时机：
     * - connected_cb: WebSocket连接建立成功时
     * - asr_cb: 收到ASR识别结果时（中间结果或最终结果）
     * - dialogue_cb: 收到对话响应时
     * - error_cb: 发生错误时（网络、协议、超时等）
     * - complete_cb: 整个识别会话完成时
     */
    void setCallbacks(ConnectedCallback connected_cb,
                     AsrCallback asr_cb,
                     DialogueCallback dialogue_cb,
                     ErrorCallback error_cb,
                     CompleteCallback complete_cb) {
        // 使用互斥锁保护回调设置，确保线程安全
        std::lock_guard<std::mutex> lock(*callback_mutex_);

        // 保存回调函数指针
        // 允许传入 nullptr，表示忽略该类型回调
        connected_callback_ = connected_cb;
        asr_callback_ = asr_cb;
        dialogue_callback_ = dialogue_cb;
        error_callback_ = error_cb;
        complete_callback_ = complete_cb;

        ESP_LOGI(TAG, "Callbacks set successfully");
    }

private:
    /**
     * @brief WebSocket客户端
     * 封装底层通信细节
     */
    AsrWebsocket websocket_;

    /**
     * @brief 连接成功回调
     * WebSocket连接建立时调用，对应 Android ASRIntelligentDialogue.onConnected()
     */
    ConnectedCallback connected_callback_;

    /**
     * @brief ASR回调
     * 处理语音识别结果（中间和最终），对应 Android onAsrMidResult/onAsrFinalResult
     */
    AsrCallback asr_callback_;

    /**
     * @brief 对话回调
     * 处理完整对话响应，对应 Android ASRIntelligentDialogue.onDialogueResult()
     */
    DialogueCallback dialogue_callback_;

    /**
     * @brief 错误回调
     * 处理各类错误，对应 Android ASRIntelligentDialogue.onError()
     */
    ErrorCallback error_callback_;

    /**
     * @brief 完成回调
     * 整个识别会话完成时调用，对应 Android ASRIntelligentDialogue.onComplete()
     */
    CompleteCallback complete_callback_;

    /**
     * @brief 状态互斥锁
     * 保护is_recognizing_和is_connected_等状态变量
     */
    std::unique_ptr<std::mutex> state_mutex_;

    /**
     * @brief 回调互斥锁
     * 保护回调函数指针
     */
    std::unique_ptr<std::mutex> callback_mutex_;

    /**
     * @brief 是否正在识别
     * true: start()成功，stop()前保持true
     * false: 空闲或已停止
     */
    bool is_recognizing_ = false;

    /**
     * @brief WebSocket连接状态
     * true: 已连接，可发送数据
     * false: 未连接或连接中
     */
    bool is_connected_ = false;

    /**
     * @brief 接收任务句柄
     * 用于处理服务器响应
     */
    TaskHandle_t receive_task_ = nullptr;

    /**
     * @brief 接收任务入口函数
     * @param param 用户参数（impl_指针）
     *
     * 任务职责：
     * 1. 等待WebSocket消息
     * 2. 解析JSON响应
     * 3. 区分响应类型（ASR中间结果、最终结果、对话指令）
     * 4. 调用相应回调函数
     * 5. 处理连接断开和重连
     *
     * 任务生命周期：
     * - start()时创建，stop()时删除
     * - 阻塞接收消息，不占用CPU
     * - 支持任务通知机制（关闭时唤醒）
     */
    static void ReceiveTaskEntry(void* param);

    /**
     * @brief 解析服务器消息
     * @param payload 消息体
     * @param len 消息长度
     *
     * 消息类型：
     * 1. mid_result: {"type": "mid_result", "result": "部分文本"}
     * 2. fin_result: {"type": "fin_result", "result": "完整文本"}
     * 3. inside_rc: {"type": "inside_rc", "data": {...}}
     * 4. dcs_decide: {"type": "dcs_decide", "end": 1}
     *
     * 实现步骤：
     * 1. JSON解析（使用cJSON）
     * 2. 提取type字段
     * 3. 根据类型分发处理
     * 4. 构造AsrResult或DialogueResult
     * 5. 调用回调函数
     * 6. 释放JSON对象
     *
     * 错误处理：
     * - JSON解析失败：记录日志，调用error_callback_
     * - 缺少必要字段：视为错误消息
     * - 未知类型：忽略或记录警告
     */
    void parseMessage(const uint8_t* payload, size_t len);

    /**
     * @brief 发送 Start Signal
     *
     * 与 Android DealSotaOne.createStartSignal() 对应
     * 建联成功后发送，包含设备信息和配置参数
     *
     * JSON 结构：
     * {
     *   "type": "start",
     *   "data": {
     *     "cuid": "设备号",
     *     "format": "pcm",
     *     "sample": 16000,
     *     "dialog_request_id": "唯一ID",
     *     "client_context": [...]
     *   }
     * }
     */
    void sendStartSignal();
};

// 单例实现
AsrDialogue& AsrDialogue::getInstance() {
    static AsrDialogue instance;
    return instance;
}

// 构造函数
AsrDialogue::AsrDialogue() : impl_(std::make_unique<Impl>()) {}

// 析构函数
AsrDialogue::~AsrDialogue() = default;

// 公有接口转发到实现类
void AsrDialogue::setCallbacks(ConnectedCallback connected_cb,
                              AsrCallback asr_cb,
                              DialogueCallback dialogue_cb,
                              ErrorCallback error_cb,
                              CompleteCallback complete_cb) {
    impl_->setCallbacks(connected_cb, asr_cb, dialogue_cb, error_cb, complete_cb);
}

bool AsrDialogue::start() {
    return impl_->start();
}

void AsrDialogue::stop() {
    impl_->stop();
}

void AsrDialogue::sendAudio(const uint8_t* data, size_t len) {
    impl_->sendAudio(data, len);
}

// 接收任务入口函数空实现
void AsrDialogue::Impl::ReceiveTaskEntry(void* param) {
    // TODO: 实现接收任务
    // - 等待WebSocket消息
    // - 解析JSON响应
    // - 调用相应回调函数
}

// 消息解析函数实现
// 参考 Android ASRIntelligentDialogue.dialogueResultParseDialogueResult() 实现
void AsrDialogue::Impl::parseMessage(const uint8_t* payload, size_t len) {
    // 将 payload 转换为字符串（用于日志和解析）
    std::string json_str(reinterpret_cast<const char*>(payload), len);

    // 解析 JSON
    cJSON* root = cJSON_Parse(json_str.c_str());
    if (!root) {
        ESP_LOGE(TAG, "Failed to parse JSON message: %s", json_str.c_str());
        if (error_callback_) {
            error_callback_(-1, "Failed to parse JSON message");
        }
        return;
    }

    // 获取消息类型
    cJSON* type_item = cJSON_GetObjectItem(root, "type");
    if (!type_item || !cJSON_IsString(type_item)) {
        ESP_LOGW(TAG, "Message missing 'type' field: %s", json_str.c_str());
        cJSON_Delete(root);
        return;
    }

    const char* type = type_item->valuestring;
    ESP_LOGI(TAG, "Received message type: %s", type);

    // ========================================
    // 根据消息类型分发处理
    // 与 Android DealSotaOne.onMessage() 对齐
    // ========================================

    if (strcmp(type, "mid_result") == 0) {
        // ASR 中间结果（实时识别）
        // 对应 Android: listener?.onMidResult(result)
        cJSON* result = cJSON_GetObjectItem(root, "result");
        if (result && cJSON_IsString(result)) {
            ESP_LOGI(TAG, "ASR mid_result: %s", result->valuestring);
            if (asr_callback_) {
                AsrResult asr_result;
                asr_result.text = result->valuestring;
                asr_result.is_final = false;
                asr_callback_(asr_result);
            }
        }
    }
    else if (strcmp(type, "fin_result") == 0) {
        // ASR 最终结果
        // 对应 Android: listener?.onFinalResult(result)
        cJSON* result = cJSON_GetObjectItem(root, "result");
        if (result && cJSON_IsString(result)) {
            ESP_LOGI(TAG, "ASR fin_result: %s", result->valuestring);
            if (asr_callback_) {
                AsrResult asr_result;
                asr_result.text = result->valuestring;
                asr_result.is_final = true;
                asr_callback_(asr_result);
            }
        }
    }
    else if (strcmp(type, "inside_rc") == 0) {
        // ========================================
        // 智能对话结果
        // 对应 Android: dialogueResultParseDialogueResult(listener, result)
        // ========================================
        cJSON* data = cJSON_GetObjectItem(root, "data");
        if (data) {
            // 解析公共字段
            std::string qid_str;
            int is_end_val = 0;

            cJSON* qid = cJSON_GetObjectItem(data, "qid");
            if (qid && cJSON_IsString(qid)) {
                qid_str = qid->valuestring;
            }

            cJSON* is_end = cJSON_GetObjectItem(data, "is_end");
            if (is_end && cJSON_IsNumber(is_end)) {
                is_end_val = is_end->valueint;
            }

            // 解析 directives 数组（与 Android 一致，data 下的 data 数组）
            cJSON* directives = cJSON_GetObjectItem(data, "data");
            if (directives && cJSON_IsArray(directives)) {
                // 遍历每个 directive，逐个调用回调
                // 对应 Android: for (i in 0 until directives.length())
                int directives_count = cJSON_GetArraySize(directives);
                for (int i = 0; i < directives_count; i++) {
                    cJSON* directive = cJSON_GetArrayItem(directives, i);
                    if (!directive) continue;

                    cJSON* header = cJSON_GetObjectItem(directive, "header");
                    cJSON* payload_obj = cJSON_GetObjectItem(directive, "payload");

                    // 构建 DialogueResult（与 Android DialogueResult 结构对齐）
                    DialogueResult dialogue_result;
                    dialogue_result.qid = qid_str;
                    dialogue_result.is_end = is_end_val;

                    // 序列化 header 为 JSON 字符串
                    // 对应 Android: DialogueResult(header = header, ...)
                    if (header) {
                        char* header_str = cJSON_PrintUnformatted(header);
                        if (header_str) {
                            dialogue_result.header = header_str;
                            free(header_str);
                        }

                        // 提取 name 作为便捷字段
                        cJSON* name = cJSON_GetObjectItem(header, "name");
                        if (name && cJSON_IsString(name)) {
                            dialogue_result.directive = name->valuestring;
                            ESP_LOGI(TAG, "Directive[%d]: %s", i, dialogue_result.directive.c_str());
                        }
                    }

                    // 序列化 payload 为 JSON 字符串
                    // 对应 Android: DialogueResult(payload = payload, ...)
                    if (payload_obj) {
                        char* payload_str = cJSON_PrintUnformatted(payload_obj);
                        if (payload_str) {
                            dialogue_result.payload = payload_str;
                            free(payload_str);
                        }
                    }

                    // 每个 directive 都调用一次回调
                    // 对应 Android: listener?.onDialogueResult(DialogueResult(...))
                    if (dialogue_callback_) {
                        dialogue_callback_(dialogue_result);
                    }
                }
            } else {
                // 没有 directives 数组时也调用回调（兼容处理）
                // 对应 Android: if (directives == null) { listener?.onDialogueResult(...) }
                DialogueResult dialogue_result;
                dialogue_result.qid = qid_str;
                dialogue_result.is_end = is_end_val;
                if (dialogue_callback_) {
                    dialogue_callback_(dialogue_result);
                }
            }

            // ========================================
            // is_end == 1 时解析 assistant_answer
            // 对应 Android: if (isEnd == 1) { ... listener?.onDialogueResult(...) }
            // ========================================
            if (is_end_val == 1) {
                cJSON* assistant_answer = cJSON_GetObjectItem(data, "assistant_answer");
                if (assistant_answer && cJSON_IsString(assistant_answer)) {
                    std::string answer_content = assistant_answer->valuestring;

                    // 尝试解析 assistant_answer 中的 content 字段
                    // 对应 Android: getAnswer(assistantAnswer)
                    cJSON* answer_json = cJSON_Parse(answer_content.c_str());
                    if (answer_json) {
                        cJSON* content = cJSON_GetObjectItem(answer_json, "content");
                        if (content && cJSON_IsString(content)) {
                            answer_content = content->valuestring;
                        }
                        cJSON_Delete(answer_json);
                    }

                    ESP_LOGI(TAG, "Session end, assistant_answer: %s", answer_content.c_str());

                    // 发送 is_end=1 的回调
                    DialogueResult end_result;
                    end_result.qid = qid_str;
                    end_result.is_end = 1;
                    end_result.assistant_answer_content = answer_content;

                    if (dialogue_callback_) {
                        dialogue_callback_(end_result);
                    }
                }
            }
        }
    }
    else if (strcmp(type, "ready") == 0) {
        // 服务器准备就绪
        ESP_LOGI(TAG, "Server ready");
    }
    else if (strcmp(type, "dcs_decide") == 0) {
        // 决策消息，检查 end 标志
        // 对应 Android: if (json.optInt("end", 0) == 1) { ... }
        cJSON* end = cJSON_GetObjectItem(root, "end");
        if (end && cJSON_IsNumber(end) && end->valueint == 1) {
            ESP_LOGI(TAG, "Session complete (dcs_decide end=1)");
            if (complete_callback_) {
                complete_callback_();
            }
        }
    }
    else if (strstr(json_str.c_str(), "directive") != nullptr) {
        // ========================================
        // 处理独立的 directive 消息
        // 对应 Android: if (text.contains("directive")) { ... }
        // ========================================
        cJSON* directive = cJSON_GetObjectItem(root, "directive");
        if (directive) {
            cJSON* header = cJSON_GetObjectItem(directive, "header");
            cJSON* payload_obj = cJSON_GetObjectItem(directive, "payload");

            DialogueResult dialogue_result;

            if (header) {
                char* header_str = cJSON_PrintUnformatted(header);
                if (header_str) {
                    dialogue_result.header = header_str;
                    free(header_str);
                }

                cJSON* namespace_item = cJSON_GetObjectItem(header, "namespace");
                cJSON* name = cJSON_GetObjectItem(header, "name");
                if (namespace_item && cJSON_IsString(namespace_item) &&
                    name && cJSON_IsString(name)) {
                    dialogue_result.directive = std::string(namespace_item->valuestring) +
                                                "." + name->valuestring;
                    ESP_LOGI(TAG, "Standalone directive: %s", dialogue_result.directive.c_str());
                }
            }

            if (payload_obj) {
                char* payload_str = cJSON_PrintUnformatted(payload_obj);
                if (payload_str) {
                    dialogue_result.payload = payload_str;
                    free(payload_str);
                }
            }

            if (dialogue_callback_) {
                dialogue_callback_(dialogue_result);
            }
        }
    }
    else {
        // 未知消息类型
        ESP_LOGD(TAG, "Unknown message type: %s", type);
    }

    // 释放 JSON 对象
    cJSON_Delete(root);
}

// 发送 Start Signal 实现
void AsrDialogue::Impl::sendStartSignal() {
    ESP_LOGI(TAG, "Building Start Signal...");

    // 创建根 JSON 对象
    cJSON* root = cJSON_CreateObject();
    if (!root) {
        ESP_LOGE(TAG, "Failed to create JSON root object");
        return;
    }

    // 设置消息类型为 "start"
    cJSON_AddStringToObject(root, "type", "start");

    // 创建 data 对象
    cJSON* data = cJSON_CreateObject();
    if (!data) {
        ESP_LOGE(TAG, "Failed to create JSON data object");
        cJSON_Delete(root);
        return;
    }

    // 获取设备配置信息
    const auto& config = AIAssistantManager::getInstance().config();

    // 基础配置（与 Android hht_ctx4.conf 对应）
    cJSON_AddStringToObject(data, "cuid", config.deviceNo.c_str());
    cJSON_AddStringToObject(data, "format", "pcm");  // ESP32 使用 PCM 格式
    cJSON_AddNumberToObject(data, "sample", 16000);
    cJSON_AddNumberToObject(data, "support_dcs", 2);
    cJSON_AddNumberToObject(data, "chunk_size", 5120);
    cJSON_AddBoolToObject(data, "support_tts", true);
    cJSON_AddBoolToObject(data, "support_text2dcs", true);
    cJSON_AddStringToObject(data, "client_ip", "");
    cJSON_AddBoolToObject(data, "access_rc", true);
    cJSON_AddBoolToObject(data, "support_part_tts", true);
    cJSON_AddBoolToObject(data, "need_stoplisten", true);
    cJSON_AddBoolToObject(data, "need_dialogue_finish", true);
    cJSON_AddBoolToObject(data, "result_trans2directive", false);

    // 生成唯一对话 ID（UUID + 时间戳）
    char dialog_id[64];
    int64_t timestamp = esp_timer_get_time() / 1000;  // 微秒转毫秒
    snprintf(dialog_id, sizeof(dialog_id), "esp32-%s-%lld", 
             config.deviceNo.c_str(), (long long)timestamp);
    cJSON_AddStringToObject(data, "dialog_request_id", dialog_id);

    // 创建 client_context 数组（设备状态信息）
    cJSON* client_context = cJSON_CreateArray();
    if (client_context) {
        // 1. Voice Output State
        cJSON* voice_state = cJSON_CreateObject();
        cJSON* voice_header = cJSON_CreateObject();
        cJSON_AddStringToObject(voice_header, "namespace", "ai.fxzsos.device_interface.voice_output");
        cJSON_AddStringToObject(voice_header, "name", "SpeechState");
        cJSON_AddItemToObject(voice_state, "header", voice_header);
        cJSON* voice_payload = cJSON_CreateObject();
        cJSON_AddStringToObject(voice_payload, "playerActivity", "FINISHED");
        cJSON_AddNumberToObject(voice_payload, "offsetInMilliseconds", 0);
        cJSON_AddStringToObject(voice_payload, "token", "");
        cJSON_AddItemToObject(voice_state, "payload", voice_payload);
        cJSON_AddItemToArray(client_context, voice_state);

        // 2. Speaker Controller State
        cJSON* speaker_state = cJSON_CreateObject();
        cJSON* speaker_header = cJSON_CreateObject();
        cJSON_AddStringToObject(speaker_header, "namespace", "ai.fxzsos.device_interface.speaker_controller");
        cJSON_AddStringToObject(speaker_header, "name", "Volume");
        cJSON_AddItemToObject(speaker_state, "header", speaker_header);
        cJSON* speaker_payload = cJSON_CreateObject();
        cJSON_AddNumberToObject(speaker_payload, "volume", 50);
        cJSON_AddBoolToObject(speaker_payload, "muted", false);
        cJSON_AddItemToObject(speaker_state, "payload", speaker_payload);
        cJSON_AddItemToArray(client_context, speaker_state);

        // 3. Audio Player State
        cJSON* audio_state = cJSON_CreateObject();
        cJSON* audio_header = cJSON_CreateObject();
        cJSON_AddStringToObject(audio_header, "namespace", "ai.fxzsos.device_interface.audio_player");
        cJSON_AddStringToObject(audio_header, "name", "PlaybackState");
        cJSON_AddItemToObject(audio_state, "header", audio_header);
        cJSON* audio_payload = cJSON_CreateObject();
        cJSON_AddStringToObject(audio_payload, "playerActivity", "FINISHED");
        cJSON_AddNumberToObject(audio_payload, "offsetInMilliseconds", 0);
        cJSON_AddStringToObject(audio_payload, "token", "");
        cJSON_AddItemToObject(audio_state, "payload", audio_payload);
        cJSON_AddItemToArray(client_context, audio_state);

        cJSON_AddItemToObject(data, "client_context", client_context);
    }

    // 将 data 添加到 root
    cJSON_AddItemToObject(root, "data", data);

    // 序列化 JSON
    char* json_str = cJSON_PrintUnformatted(root);
    if (json_str) {
        ESP_LOGI(TAG, "Sending Start Signal: %s", json_str);

        // 发送到服务器
        if (!websocket_.sendText(json_str)) {
            ESP_LOGW(TAG, "Failed to send Start Signal");
        } else {
            ESP_LOGI(TAG, "Start Signal sent successfully");
        }

        // 释放 JSON 字符串
        free(json_str);
    } else {
        ESP_LOGE(TAG, "Failed to serialize JSON");
    }

    // 释放 JSON 对象
    cJSON_Delete(root);
}

}  // namespace ai_sdk
