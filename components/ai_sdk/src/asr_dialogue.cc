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
#include "esp_log.h"
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
        static const char* TAG = "AsrDialogue.Start";

        // 加锁保护状态
        std::lock_guard<std::mutex> lock(*state_mutex_);

        // 第1步：检查状态（如果已在识别中，返回false）
        if (is_recognizing_) {
            ESP_LOGW(TAG, "Already recognizing, cannot start again");
            return false;
        }

        ESP_LOGI(TAG, "Starting ASR recognition...");

        // 第2步：构建完整WebSocket URL（与Android一致）
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

        // 第3步：配置WebSocket参数（与Android超时配置一致）
        // - 连接超时：10秒
        // - 网络超时：30秒
        // - 心跳间隔：30秒
        AsrWebsocketConfig config;
        config.url = ws_url;
        config.connect_timeout_ms = 10000;   // 10秒
        config.network_timeout_ms = 30000;   // 30秒
        config.ping_interval_ms = 30000;     // 30秒（心跳）
        config.buffer_size = 4096;             // 4KB缓冲区
        ESP_LOGI(TAG, "WebSocket config created: timeout=%d/%d/%d ms",
                 config.connect_timeout_ms,
                 config.network_timeout_ms,
                 config.ping_interval_ms);

        // 第4步：重置信号量（确保在获取前为0）
        if (connection_semaphore_ != nullptr) {
            xSemaphoreTake(connection_semaphore_, 0);  // 清空信号量
        }

        // 第5步：建立WebSocket连接（异步）
        // 注册消息回调，监听 "CONNECTED" 事件
        websocket_.setMessageCallback([this](const uint8_t* data, size_t len, int type) {
            // 只处理文本消息（type为0代表文本，1代表二进制）
            if (type == 0) {
                std::string msg(reinterpret_cast<const char*>(data), len);
                ESP_LOGD(TAG, "Received message: %s", msg.c_str());
                if (msg == "CONNECTED") {
                    ESP_LOGI(TAG, "Received connection confirmation from WebSocket");
                    this->onConnected();
                }
            }
        });

        if (!websocket_.connect(config)) {
            ESP_LOGE(TAG, "Failed to initiate WebSocket connection");
            if (error_callback_) {
                error_callback_(-1, "Failed to initiate WebSocket connection");
            }
            return false;
        }

        // 第6步：等待连接成功（模拟 Android CountDownLatch.await()）
        // 等待事件：EventHandler 在连接成功时 give() 信号量
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

        // 检查是否真的连接成功
        if (!is_connected_) {
            ESP_LOGE(TAG, "Connection failed");
            websocket_.disconnect();
            if (error_callback_) {
                error_callback_(-1, "Connection failed");
            }
            return false;
        }

        ESP_LOGI(TAG, "WebSocket connected successfully!");

        // 第7步：调用连接成功回调（对应 Android onConnected()）
        if (connected_callback_) {
            connected_callback_();
        }

        // 第8步：发送ASR配置参数
        // 格式与服务器要求的配置一致
        std::string asr_config = R"({
            "type": "config",
            "format": "pcm",
            "sample_rate": 16000,
            "channels": 1,
            "bits_per_sample": 16
        })";
        ESP_LOGI(TAG, "Sending ASR config: %s", asr_config.c_str());

        if (!websocket_.sendText(asr_config)) {
            ESP_LOGW(TAG, "Failed to send ASR config, but continuing...");
            // 发送配置失败但继续（非致命错误）
        }

        // 第9步：标记为识别中状态
        is_recognizing_ = true;
        ESP_LOGI(TAG, "ASR recognition started successfully");

        // TODO: 第10步：启动接收任务（处理服务器响应）
        // xTaskCreate(ReceiveTaskEntry, "asr_receive", RECEIVE_TASK_STACK, this, ...);

        return true;
    }

    /**
     * @brief WebSocket连接成功回调
     *
     * 由 SetMessageCallback 在收到 "CONNECTED" 消息时调用
     * EventHandler 在 WEBSOCKET_EVENT_CONNECTED 时发送 "CONNECTED"
     */
    void onConnected() {
        static const char* TAG = "AsrDialogue.onConnected";
        ESP_LOGI(TAG, "WebSocket connected");

        std::lock_guard<std::mutex> lock(*state_mutex_);
        is_connected_ = true;

        // 释放信号量，唤醒等待的 start() 函数
        if (connection_semaphore_ != nullptr) {
            xSemaphoreGive(connection_semaphore_);
        }
    }

    /**
     * @brief WebSocket断开回调
     * @param error 错误信息
     */
    void onDisconnected(const std::string& error = "") {
        static const char* TAG = "AsrDialogue.onDisconnected";
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
        // TODO: 实现停止逻辑
        // std::lock_guard<std::mutex> lock(*state_mutex_);
        //
        // if (!is_recognizing_) {
        //     ESP_LOGW(TAG, "Not recognizing");
        //     return;
        // }
        //
        // TODO: 发送结束信号
        // std::string finish_msg = R"({"type": "finish"})";
        // websocket_.sendText(finish_msg);
        //
        // TODO: 等待服务器响应（超时2秒）
        // vTaskDelay(pdMS_TO_TICKS(2000));
        //
        // TODO: 关闭连接
        // websocket_.disconnect();
        //
        // is_recognizing_ = false;
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
        // TODO: 使用互斥锁保护回调设置
        // std::lock_guard<std::mutex> lock(callback_mutex_);
        // connected_callback_ = connected_cb;
        // asr_callback_ = asr_cb;
        // dialogue_callback_ = dialogue_cb;
        // error_callback_ = error_cb;
        // complete_callback_ = complete_cb;
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
     * 2. final_result: {"type": "final_result", "result": "完整文本"}
     * 3. directive: {"type": "directive", "data": [...]}
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

// 消息解析函数空实现
void AsrDialogue::Impl::parseMessage(const uint8_t* payload, size_t len) {
    // TODO: 实现消息解析
    // - 解析JSON格式
    // - 区分响应类型（ASR中间结果、最终结果、对话指令）
    // - 调用回调函数
}

}  // namespace ai_sdk
